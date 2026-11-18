package org.reignsstudio.storyland;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;

public final class StorylandGameImport {
    public static final int MAX_DEPTH = 24;
    public static final int MAX_ENTRIES = 100000;
    public static final int BUFFER_BYTES = 256 * 1024;
    public static final long RESERVE_BYTES = 256L * 1024 * 1024;

    public interface Cancellation {
        void check() throws IOException;
    }

    public interface Storage {
        long availableBytes() throws IOException;
    }

    public interface Progress {
        void scanned(String name, long bytes, int files, boolean measuring);
        void copied(String name, long bytes, long totalBytes, int files, int totalFiles);
    }

    public interface Source {
        List<Entry> children(String documentId) throws IOException;
        InputStream open(String documentId) throws IOException;
    }

    public static final class Entry {
        public final String id;
        public final String name;
        public final boolean directory;
        public final long bytes;

        public Entry(String id, String name, boolean directory, long bytes) {
            this.id = id;
            this.name = name;
            this.directory = directory;
            this.bytes = bytes;
        }
    }

    public static final class PlannedFile {
        public final String id;
        public final String relativePath;
        public final long bytes;

        PlannedFile(String id, String relativePath, long bytes) {
            this.id = id;
            this.relativePath = relativePath;
            this.bytes = bytes;
        }
    }

    public static final class Plan {
        public final List<PlannedFile> files;
        public final List<String> directories;
        public final long totalBytes;
        public final long storageBytes;
        public final long requiredBytes;
        public final long blockBytes;
        public final int measuredFiles;

        Plan(Planner planner) throws IOException {
            files = Collections.unmodifiableList(new ArrayList<>(planner.files));
            directories = Collections.unmodifiableList(new ArrayList<>(planner.directories));
            totalBytes = planner.totalBytes;
            storageBytes = planner.storageBytes;
            requiredBytes = add(storageBytes, RESERVE_BYTES);
            blockBytes = planner.blockBytes;
            measuredFiles = planner.measuredFiles;
        }
    }

    public static final class Installed {
        public final long bytes;
        public final int files;

        Installed(long bytes, int files) {
            this.bytes = bytes;
            this.files = files;
        }
    }

    private StorylandGameImport() { }

    public static String filenameKey(String name) throws IOException {
        if (name == null || name.isEmpty() || name.length() > 255 || name.equals(".") || name.equals("..") ||
            name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 ||
            name.getBytes(StandardCharsets.UTF_8).length > 255)
            throw new IOException("The selected folder contains an invalid filename.");
        for (int index = 0; index < name.length(); ++index) {
            char character = name.charAt(index);
            if (Character.isISOControl(character))
                throw new IOException("The selected folder contains a control character in a filename.");
            if (Character.isHighSurrogate(character)) {
                if (index + 1 >= name.length() || !Character.isLowSurrogate(name.charAt(index + 1)))
                    throw new IOException("The selected folder contains an invalid Unicode filename.");
                ++index;
            } else if (Character.isLowSurrogate(character)) {
                throw new IOException("The selected folder contains an invalid Unicode filename.");
            }
        }
        return Normalizer.normalize(name, Normalizer.Form.NFC).toLowerCase(Locale.ROOT);
    }

    public static long add(long first, long second) throws IOException {
        if (first < 0 || second < 0 || first > Long.MAX_VALUE - second)
            throw new IOException("The selected game reports an invalid or excessive storage size.");
        return first + second;
    }

    public static long allocatedBytes(long bytes, long blockBytes) throws IOException {
        if (bytes < 0 || blockBytes <= 0)
            throw new IOException("Android reported an invalid file or filesystem block size.");
        long rounded = add(bytes, blockBytes - 1) / blockBytes;
        if (rounded > Long.MAX_VALUE / blockBytes)
            throw new IOException("The selected game reports an excessive storage size.");
        return Math.max(blockBytes, rounded * blockBytes);
    }

    public static String sizeText(long bytes) {
        if (bytes >= 1024L * 1024 * 1024)
            return String.format(Locale.ROOT, "%.2f GiB", bytes / (1024.0 * 1024 * 1024));
        return String.format(Locale.ROOT, "%.1f MiB", bytes / (1024.0 * 1024));
    }

    public static String spaceDescription(Plan plan, long available) {
        return "Game files: " + sizeText(plan.totalBytes) + " (" + plan.files.size() + " files)\n" +
            "Required free space: " + sizeText(plan.requiredBytes) + "\n" +
            "Includes filesystem allocation and a " + sizeText(RESERVE_BYTES) + " free reserve.\n" +
            "Available now: " + sizeText(Math.max(0, available));
    }

    public static void requireSpace(Plan plan, long available) throws IOException {
        if (available < plan.requiredBytes)
            throw new IOException("Not enough free storage to import this game.\n\n" +
                spaceDescription(plan, available));
    }

    public static Plan plan(Source source, String documentId, long blockBytes,
        Cancellation cancellation, Progress progress) throws IOException {
        if (documentId == null || documentId.isEmpty())
            throw new IOException("Android returned an invalid game folder.");
        Planner planner = new Planner(source, blockBytes, cancellation, progress);
        planner.visit(documentId, "", 0);
        if (!planner.gameFound)
            throw new IOException("The selected folder does not contain a nonempty GAME.DTZ in a supported game folder. " +
                "Choose the extracted game root so its world, scripts, animations and audio stay together.");
        return new Plan(planner);
    }

    private static final class Planner {
        final Source source;
        final long blockBytes;
        final Cancellation cancellation;
        final Progress progress;
        final List<PlannedFile> files = new ArrayList<>();
        final List<String> directories = new ArrayList<>();
        final Set<String> visited = new HashSet<>();
        final byte[] buffer = new byte[BUFFER_BYTES];
        int entries;
        int measuredFiles;
        long totalBytes;
        long storageBytes;
        boolean gameFound;

        Planner(Source source, long blockBytes, Cancellation cancellation, Progress progress) throws IOException {
            if (blockBytes <= 0) throw new IOException("Android reported an invalid filesystem block size.");
            this.source = source;
            this.blockBytes = blockBytes;
            this.cancellation = cancellation;
            this.progress = progress;
            storageBytes = blockBytes;
        }

        void visit(String documentId, String relative, int depth) throws IOException {
            cancellation.check();
            if (depth > MAX_DEPTH) throw new IOException("The selected folder is nested too deeply.");
            if (!visited.add(documentId)) throw new IOException("The document provider returned a folder loop.");
            List<Entry> children = source.children(documentId);
            if (children == null) throw new IOException("Android could not list a game folder.");
            Set<String> names = new HashSet<>();
            for (Entry entry : children) {
                cancellation.check();
                if (entry == null || entry.id == null || entry.id.isEmpty())
                    throw new IOException("Android returned an invalid game file.");
                if (++entries > MAX_ENTRIES)
                    throw new IOException("The selected folder contains too many files or folders.");
                if (!names.add(filenameKey(entry.name)))
                    throw new IOException("Names differ only by case or Unicode spelling: " + entry.name);
            }
            ArrayList<Entry> ordered = new ArrayList<>(children);
            Collections.sort(ordered, (first, second) -> first.name.compareTo(second.name));
            for (Entry entry : ordered) {
                cancellation.check();
                String path = relative.isEmpty() ? entry.name : relative + "/" + entry.name;
                if (entry.directory) {
                    directories.add(path);
                    storageBytes = add(storageBytes, blockBytes);
                    visit(entry.id, path, depth + 1);
                } else {
                    long bytes = entry.bytes;
                    if (bytes < 0) {
                        bytes = measure(entry.id, path);
                        ++measuredFiles;
                    }
                    totalBytes = add(totalBytes, bytes);
                    storageBytes = add(storageBytes, allocatedBytes(bytes, blockBytes));
                    files.add(new PlannedFile(entry.id, path, bytes));
                    if (bytes > 0 && gameDtzPath(path)) gameFound = true;
                    progress.scanned(path, totalBytes, files.size(), false);
                }
            }
        }

        long measure(String documentId, String path) throws IOException {
            long bytes = 0;
            try (InputStream input = source.open(documentId)) {
                if (input == null) throw new IOException("Cannot read the size of " + path + ".");
                for (;;) {
                    cancellation.check();
                    int count = read(input, buffer);
                    if (count < 0) break;
                    bytes = add(bytes, count);
                    progress.scanned(path, add(totalBytes, bytes), files.size(), true);
                }
            }
            return bytes;
        }
    }

    private static int read(InputStream input, byte[] buffer) throws IOException {
        int count = input.read(buffer);
        if (count != 0) return count;
        int value = input.read();
        if (value < 0) return -1;
        buffer[0] = (byte)value;
        return 1;
    }

    private static File child(File root, String relative) throws IOException {
        File file = new File(root, relative).getCanonicalFile();
        if (!file.getPath().startsWith(root.getPath() + File.separator))
            throw new IOException("A game filename would leave its import folder.");
        return file;
    }

    private static void requireWriteSpace(Storage storage, long bytes, long blockBytes) throws IOException {
        long required = add(RESERVE_BYTES, allocatedBytes(bytes, blockBytes));
        long available = storage.availableBytes();
        if (available < required)
            throw new IOException("Available storage changed during import. " + sizeText(available) +
                " remains; at least " + sizeText(required) + " is needed for the next write and reserve.");
    }

    public static void copy(Plan plan, Source source, File destination, Storage storage,
        Cancellation cancellation, Progress progress) throws IOException {
        cancellation.check();
        requireSpace(plan, storage.availableBytes());
        File root = destination.getCanonicalFile();
        File absolute = destination.getAbsoluteFile();
        File direct = new File(absolute.getParentFile().getCanonicalFile(), absolute.getName());
        if (!root.equals(direct.getAbsoluteFile()))
            throw new IOException("The new import folder is a linked path.");
        File[] existing = root.listFiles();
        if (!root.isDirectory() || existing == null || existing.length != 0)
            throw new IOException("The new import folder is not empty or cannot be read.");
        byte[] buffer = new byte[BUFFER_BYTES];
        for (String relative : plan.directories) {
            cancellation.check();
            requireWriteSpace(storage, 0, plan.blockBytes);
            if (!child(root, relative).mkdir())
                throw new IOException("Cannot create game folder: " + relative);
        }
        long copied = 0;
        int completed = 0;
        for (PlannedFile file : plan.files) {
            cancellation.check();
            requireWriteSpace(storage, 0, plan.blockBytes);
            File outputFile = child(root, file.relativePath);
            if (!outputFile.createNewFile())
                throw new IOException("A game file already exists in the new import: " + file.relativePath);
            long written = 0;
            try (InputStream input = source.open(file.id);
                 FileOutputStream output = new FileOutputStream(outputFile)) {
                if (input == null) throw new IOException("Cannot open " + file.relativePath + ".");
                for (;;) {
                    cancellation.check();
                    int count = read(input, buffer);
                    if (count < 0) break;
                    if (count > file.bytes - written)
                        throw new IOException("The source changed after its size was checked: " + file.relativePath +
                            ". Select the folder again to measure the updated files.");
                    requireWriteSpace(storage, count, plan.blockBytes);
                    output.write(buffer, 0, count);
                    written += count;
                    copied += count;
                    progress.copied(file.relativePath, copied, plan.totalBytes, completed, plan.files.size());
                }
                output.getFD().sync();
            }
            if (written != file.bytes)
                throw new IOException("Incomplete file: " + file.relativePath + ". Import it again.");
            ++completed;
            progress.copied(file.relativePath, copied, plan.totalBytes, completed, plan.files.size());
        }
        cancellation.check();
    }

    private static boolean gameDtzPath(String relative) {
        String[] parts = relative.toLowerCase(Locale.ROOT).split("/");
        if (!parts[parts.length - 1].equals("game.dtz")) return false;
        for (int index = 0; index + 1 < parts.length; ++index) {
            switch (parts[index]) {
                case "ps2": case "psp": case "chk": case "rundata": case "models":
                case "data": case "configs": case "scripts": case "anim": case "text":
                case "usrdir": case "psp_game": case "levels": case "lvz": case "img":
                    break;
                default: return false;
            }
        }
        return true;
    }

    public static Installed inspectInstalled(File directory, Cancellation cancellation) throws IOException {
        InstalledScan scan = new InstalledScan(directory.getCanonicalFile(), cancellation);
        scan.visit(scan.root, "", 0);
        return scan.gameFound ? new Installed(scan.bytes, scan.files) : null;
    }

    public static boolean isPartialName(String name) {
        if (name == null || !name.startsWith(".import-")) return false;
        String identifier = name.substring(".import-".length());
        try {
            return UUID.fromString(identifier).toString().equals(identifier);
        } catch (IllegalArgumentException exception) {
            return false;
        }
    }

    private static File partialDirectory(File games, File partial) throws IOException {
        File parent = games.getCanonicalFile();
        File candidate = new File(parent, partial.getName());
        if (!isPartialName(candidate.getName()))
            throw new IOException("This is not a recognized partial game import.");
        if (!partial.getCanonicalFile().equals(candidate.getAbsoluteFile()) ||
            !candidate.getCanonicalFile().getParentFile().equals(parent) ||
            Files.isSymbolicLink(candidate.toPath()))
            throw new IOException("The partial import is outside the installed-games folder.");
        if (candidate.exists() && !candidate.isDirectory())
            throw new IOException("The partial import is not a directory.");
        return candidate;
    }

    public static Installed inspectPartial(File games, File partial, Cancellation cancellation) throws IOException {
        File root = partialDirectory(games, partial);
        InstalledScan scan = new InstalledScan(root, cancellation);
        scan.visit(root, "", 0);
        return new Installed(scan.bytes, scan.files);
    }

    public static void removePartial(File games, File partial) throws IOException {
        removePartial(games, partial, () -> { });
    }

    public static void removePartial(File games, File partial, Cancellation cancellation) throws IOException {
        removePartialContents(partialDirectory(games, partial), cancellation);
    }

    private static void removePartialContents(File file, Cancellation cancellation) throws IOException {
        cancellation.check();
        if (!file.exists() && !Files.isSymbolicLink(file.toPath())) return;
        if (file.isDirectory() && file.getCanonicalFile().equals(file.getAbsoluteFile())) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Cannot inspect an incomplete import for removal.");
            for (File child : children) removePartialContents(child, cancellation);
        }
        if (!file.delete()) throw new IOException("Cannot remove an incomplete game import.");
    }

    private static final class InstalledScan {
        final File root;
        final Cancellation cancellation;
        final Set<String> visited = new HashSet<>();
        long bytes;
        int entries;
        int files;
        boolean gameFound;

        InstalledScan(File root, Cancellation cancellation) {
            this.root = root;
            this.cancellation = cancellation;
        }

        void visit(File directory, String relative, int depth) throws IOException {
            cancellation.check();
            if (depth > MAX_DEPTH || !visited.add(directory.getCanonicalPath()))
                throw new IOException("An installed game contains a folder loop or excessive nesting.");
            File[] children = directory.listFiles();
            if (children == null) throw new IOException("Cannot read an installed game folder.");
            for (File file : children) {
                cancellation.check();
                if (++entries > MAX_ENTRIES)
                    throw new IOException("An installed game contains too many files or folders.");
                File canonical = file.getCanonicalFile();
                if (!canonical.equals(file.getAbsoluteFile()) ||
                    !canonical.getPath().startsWith(root.getPath() + File.separator))
                    throw new IOException("An installed game contains a linked path.");
                String path = relative.isEmpty() ? file.getName() : relative + "/" + file.getName();
                if (file.isDirectory()) visit(file, path, depth + 1);
                else if (file.isFile()) {
                    long size = file.length();
                    bytes = add(bytes, size);
                    ++files;
                    if (size > 0 && gameDtzPath(path)) gameFound = true;
                }
            }
        }
    }
}
