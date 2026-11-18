package org.reignsstudio.storyland;

import org.libsdl.app.SDLActivity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.StatFs;
import android.provider.DocumentsContract;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.Locale;
import java.util.List;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.atomic.AtomicBoolean;

public final class StorylandActivity extends SDLActivity {
    private static final int OPEN_GAME = 72;
    private static final int EXPORT_FILES = 73;
    private static final int REPLACE_RESOURCE = 74;
    private static final int OPEN_ARCHIVE_COMPANION = 75;
    private static final int OPEN_ARCHIVE_FOLDER = 76;
    private static final int BROWSE_RESOURCE_FOLDER = 77;
    private int replacementToken;
    private File pendingArchive;
    private File pendingArchiveFolder;
    private String pendingArchiveName;
    private volatile float[] uiMetrics = {1,0,0,0,0};
    private File exportDirectory;
    private final AtomicBoolean importing = new AtomicBoolean(false);
    private final AtomicBoolean cancelled = new AtomicBoolean(false);
    private Thread importThread;
    private ProgressDialog progress;
    private long lastProgress;
    private long copiedBytes;
    private StorylandProgramMusic programMusic;

    private static native void openGame(String path);
    private static native void resourceImported(String path, int token, String name);
    private static native void archiveImported(String path, String companion);

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        programMusic = new StorylandProgramMusic(this);
        setVolumeControlStream(android.media.AudioManager.STREAM_MUSIC);
        getWindow().addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        uiMetrics = new float[]{getResources().getDisplayMetrics().density,0,0,0,0};
        android.view.View decor = getWindow().getDecorView();
        decor.setOnApplyWindowInsetsListener((view, insets) -> { view.post(this::updateUiMetrics); return insets; });
        decor.addOnLayoutChangeListener((view,left,top,right,bottom,oldLeft,oldTop,oldRight,oldBottom) -> updateUiMetrics());
        decor.post(this::updateUiMetrics);
    }

    private void updateUiMetrics() {
        float density = getResources().getDisplayMetrics().density;
        android.view.View decor = getWindow().getDecorView();
        android.view.View surface = mSurface;
        if (surface == null || surface.getWidth() == 0) { uiMetrics = new float[]{density,0,0,0,0}; return; }
        android.view.WindowInsets insets = decor.getRootWindowInsets();
        if (insets == null) { uiMetrics = new float[]{density,0,0,0,0}; return; }
        int left = insets.getSystemWindowInsetLeft(), top = insets.getSystemWindowInsetTop();
        int right = insets.getSystemWindowInsetRight(), bottom = insets.getSystemWindowInsetBottom();
        if (android.os.Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
            android.view.DisplayCutout cutout = insets.getDisplayCutout();
            left = Math.max(left,cutout.getSafeInsetLeft()); top = Math.max(top,cutout.getSafeInsetTop());
            right = Math.max(right,cutout.getSafeInsetRight()); bottom = Math.max(bottom,cutout.getSafeInsetBottom());
        }
        int[] decorPosition = new int[2], surfacePosition = new int[2];
        decor.getLocationOnScreen(decorPosition); surface.getLocationOnScreen(surfacePosition);
        uiMetrics = new float[]{density,
            Math.max(0,decorPosition[0]+left-surfacePosition[0]),
            Math.max(0,decorPosition[1]+top-surfacePosition[1]),
            Math.max(0,surfacePosition[0]+surface.getWidth()-(decorPosition[0]+decor.getWidth()-right)),
            Math.max(0,surfacePosition[1]+surface.getHeight()-(decorPosition[1]+decor.getHeight()-bottom))};
    }

    public float[] getLayoutMetrics() { return uiMetrics; }

    public void setProgramMusic(int track) {
        runOnUiThread(() -> { if (programMusic != null && !isFinishing()) programMusic.play(track); });
    }

    public String getProgramMusicError() { return programMusic == null ? "" : programMusic.error(); }

    public void chooseResourceFile(int token) {
        runOnUiThread(() -> {
            if (importing.get()) return;
            replacementToken = token;
            if (token == -1) {
                String savedFolder = getPreferences(MODE_PRIVATE).getString("archive_folder_uri", "");
                if (savedFolder.isEmpty()) chooseResourceFolder();
                else browseResourceFolder(Uri.parse(savedFolder));
                return;
            }
            chooseSingleFile(token);
        });
    }

    private void chooseSingleFile(int token) {
        replacementToken = token;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivityForResult(intent, REPLACE_RESOURCE);
        } catch (android.content.ActivityNotFoundException exception) {
            message("No file picker installed.");
        }
    }

    private void chooseResourceFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
            Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try {
            startActivityForResult(intent, BROWSE_RESOURCE_FOLDER);
        } catch (android.content.ActivityNotFoundException exception) {
            chooseSingleFile(-1);
        }
    }

    private static final class FolderFile {
        final String name;
        final Uri uri;

        FolderFile(String name, Uri uri) {
            this.name = name;
            this.uri = uri;
        }
    }

    private boolean isResourceFile(String name) {
        if (isArchiveFile(name)) return true;
        String lower = name.toLowerCase(Locale.ROOT);
        for (String extension : new String[] {
            ".mdl", ".chk", ".xtx", ".tex", ".dff", ".txd",
            ".anim", ".ifp", ".bsp", ".col", ".pak", ".wbl",
            ".gxt", ".scm", ".pss", ".pmf", ".dat"
        }) {
            if (lower.endsWith(extension)) return true;
        }
        return false;
    }

    private ArrayList<FolderFile> listResourceFolder(Uri tree) throws IOException {
        ArrayList<FolderFile> files = new ArrayList<>();
        try {
            String documentId = DocumentsContract.getTreeDocumentId(tree);
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
            String[] columns = {
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE
            };
            try (Cursor cursor = getContentResolver().query(children, columns, null, null, null)) {
                if (cursor == null) throw new IOException("Can't read the selected folder.");
                while (cursor.moveToNext()) {
                    String childId = cursor.getString(0);
                    String name = cursor.getString(1);
                    String mime = cursor.getString(2);
                    if (childId == null || name == null || name.isEmpty() ||
                        name.contains("/") || name.contains("\\") ||
                        DocumentsContract.Document.MIME_TYPE_DIR.equals(mime) ||
                        !isResourceFile(name)) continue;
                    Uri document = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                    files.add(new FolderFile(name, document));
                    if (files.size() >= 10000) break;
                }
            }
        } catch (RuntimeException exception) {
            throw new IOException("Android can't read this folder. Choose it again.", exception);
        }
        Collections.sort(files, (left, right) -> left.name.compareToIgnoreCase(right.name));
        return files;
    }

    private void browseResourceFolder(Uri tree) {
        if (importing.get()) return;
        new Thread(() -> {
            try {
                ArrayList<FolderFile> files = listResourceFolder(tree);
                runOnUiThread(() -> showResourceFolderFiles(files));
            } catch (IOException exception) {
                getPreferences(MODE_PRIVATE).edit().remove("archive_folder_uri").apply();
                runOnUiThread(() -> {
                    message(exception.getMessage());
                    chooseResourceFolder();
                });
            }
        }, "Storyland folder browser").start();
    }

    private void showResourceFolderFiles(ArrayList<FolderFile> files) {
        if (isFinishing()) return;
        if (files.isEmpty()) {
            new AlertDialog.Builder(this)
                .setTitle("No files found")
                .setMessage("No supported files in this folder.")
                .setPositiveButton("Other folder", (dialog, which) -> chooseResourceFolder())
                .setNegativeButton("Single file", (dialog, which) -> chooseSingleFile(-1))
                .show();
            return;
        }
        String[] names = new String[files.size()];
        for (int index = 0; index < files.size(); index++) names[index] = files.get(index).name;
        new AlertDialog.Builder(this)
            .setTitle("Open file")
            .setItems(names, (dialog, which) -> {
                if (which < 0 || which >= files.size() || importing.get()) return;
                FolderFile selected = files.get(which);
                replacementToken = -1;
                if (!isArchiveFile(selected.name)) {
                    importResource(selected.uri);
                    return;
                }
                Uri matchingFile = null;
                String[] matches = StorylandCompanionFiles.expectedNames(selected.name);
                for (String expected : matches) {
                    for (FolderFile item : files) {
                        if (expected.equalsIgnoreCase(item.name)) {
                            matchingFile = item.uri;
                            break;
                        }
                    }
                    if (matchingFile != null) break;
                }
                importPrimaryArchive(selected.uri, matchingFile);
            })
            .setNeutralButton("Other folder", (dialog, which) -> chooseResourceFolder())
            .setNegativeButton("Single file", (dialog, which) -> chooseSingleFile(-1))
            .show();
    }

    private void importResource(Uri document) {
        if (!importing.compareAndSet(false,true)) return;
        final int token = replacementToken;
        cancelled.set(false);
        importThread = new Thread(() -> {
            File outputFile = new File(new File(getFilesDir(),"imports"),UUID.randomUUID().toString());
            try {
                String displayName = "resource.bin";
                try (Cursor cursor = getContentResolver().query(document,new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)) {
                    if(cursor!=null&&cursor.moveToFirst()&&!cursor.isNull(0))displayName=cursor.getString(0);
                }
                final String resourceName = displayName;
                File folder = outputFile.getParentFile();
                if (!folder.isDirectory() && !folder.mkdirs()) throw new IOException("Cannot create the import folder.");
                try (InputStream input = getContentResolver().openInputStream(document);
                     FileOutputStream output = new FileOutputStream(outputFile)) {
                    if (input == null) throw new IOException("Cannot open this resource.");
                    byte[] buffer = new byte[65536]; int count; long total = 0;
                    while ((count=input.read(buffer)) != -1) {
                        if (cancelled.get()) throw new IOException("Cancelled.");
                        total += count;
                        if (total > 128L*1024*1024) throw new IOException("This file is too large. Open the LVZ or IMG instead.");
                        output.write(buffer,0,count);
                    }
                    if (total == 0) throw new IOException("The selected resource is empty.");
                    output.getFD().sync();
                }
                runOnUiThread(() -> { finishImport(); resourceImported(outputFile.getAbsolutePath(),token,resourceName); });
            } catch (Exception exception) {
                outputFile.delete(); String reason = exception.getMessage();
                runOnUiThread(() -> { finishImport(); message(reason); });
            }
        },"Storyland resource import");
        importThread.start();
    }

    private String resourceDisplayName(Uri document) throws IOException {
        String name = null;
        try (Cursor cursor = getContentResolver().query(document,
            new String[]{android.provider.OpenableColumns.DISPLAY_NAME},null,null,null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) name = cursor.getString(0);
        }
        if (name == null || name.isEmpty() || name.equals(".") || name.equals("..") ||
            name.contains("/") || name.contains("\\") || name.indexOf((char)0)>=0)
            throw new IOException("The selected file has an invalid name.");
        return name;
    }

    private boolean isArchiveFile(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.endsWith(".lvz") || lower.endsWith(".img") || lower.endsWith(".dtz") ||
            lower.endsWith(".dir") || lower.endsWith(".zmg") || lower.equals("game.bin");
    }

    private void chooseCompanion() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try { startActivityForResult(intent,OPEN_ARCHIVE_COMPANION); }
        catch (android.content.ActivityNotFoundException exception) {
            cancelPendingArchive();
            message("No Android file picker is installed.");
        }
    }

    private void chooseArchiveFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION |
            Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION |
            Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try {
            startActivityForResult(intent, OPEN_ARCHIVE_FOLDER);
        } catch (android.content.ActivityNotFoundException exception) {
            message("No folder picker installed.");
            chooseCompanion();
        }
    }

    private Uri findInArchiveTree(Uri tree, String selectedName, String requiredFolderId) {
        String[] expectedNames = StorylandCompanionFiles.expectedNames(selectedName);
        if (tree == null || expectedNames.length == 0) return null;

        try {
            String folderId = DocumentsContract.getTreeDocumentId(tree);
            if (requiredFolderId != null && !requiredFolderId.equals(folderId)) return null;
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, folderId);
            String[] columns = {
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                DocumentsContract.Document.COLUMN_MIME_TYPE
            };
            try (Cursor cursor = getContentResolver().query(children, columns, null, null, null)) {
                if (cursor == null) return null;
                for (String wanted : expectedNames) {
                    cursor.moveToPosition(-1);
                    while (cursor.moveToNext()) {
                        String candidateName = cursor.getString(1);
                        String candidateType = cursor.getString(2);
                        if (!wanted.equalsIgnoreCase(candidateName) ||
                            DocumentsContract.Document.MIME_TYPE_DIR.equals(candidateType)) continue;
                        String childId = cursor.getString(0);
                        if (childId != null)
                            return DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                    }
                }
            }
        } catch (SecurityException | IllegalArgumentException | UnsupportedOperationException exception) {
            return null;
        }
        return null;
    }

    private Uri findNearbyArchive(Uri selected, String selectedName) {
        String[] expectedNames = StorylandCompanionFiles.expectedNames(selectedName);
        if (expectedNames.length == 0 || !DocumentsContract.isDocumentUri(this, selected)) return null;

        String documentId;
        try {
            documentId = DocumentsContract.getDocumentId(selected);
        } catch (SecurityException | IllegalArgumentException exception) {
            return null;
        }

        int separator = documentId.lastIndexOf('/');
        String parentId;
        if (separator >= 0) {
            parentId = documentId.substring(0, separator);
        } else if ("com.android.externalstorage.documents".equals(selected.getAuthority()) &&
                   documentId.indexOf(':') >= 0) {
            parentId = documentId.substring(0, documentId.indexOf(':') + 1);
        } else {
            return null;
        }

        String savedTree = getPreferences(MODE_PRIVATE).getString("archive_folder_uri", "");
        if (!savedTree.isEmpty()) {
            Uri tree = Uri.parse(savedTree);
            if (selected.getAuthority().equals(tree.getAuthority())) {
                Uri sibling = findInArchiveTree(tree, selectedName, parentId);
                if (sibling != null) return sibling;
            }
        }

        String[] fields = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try {
            Uri children = DocumentsContract.buildChildDocumentsUri(selected.getAuthority(), parentId);
            try (Cursor cursor = getContentResolver().query(children, fields, null, null, null)) {
                if (cursor != null) {
                    for (String expected : expectedNames) {
                        cursor.moveToPosition(-1);
                        while (cursor.moveToNext()) {
                            String name = cursor.getString(1);
                            String mimeType = cursor.getString(2);
                            if (!expected.equalsIgnoreCase(name) ||
                                DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType)) continue;
                            String siblingId = cursor.getString(0);
                            if (siblingId == null || siblingId.equals(documentId)) continue;
                            return DocumentsContract.buildDocumentUri(selected.getAuthority(), siblingId);
                        }
                    }
                }
            }
        } catch (SecurityException | IllegalArgumentException | UnsupportedOperationException exception) {
            // Some file providers only allow access to the selected file.
        }

        // ExternalStorageProvider sometimes allows reading a sibling document even
        // if listing the folder is disabled. If not, the normal file picker is used.
        if ("com.android.externalstorage.documents".equals(selected.getAuthority())) {
            for (String expected : expectedNames) {
                String stem = expected.substring(0, expected.lastIndexOf('.'));
                String[] names = {expected, stem + expected.substring(expected.lastIndexOf('.')).toUpperCase(java.util.Locale.ROOT)};
                for (String name : names) {
                    String siblingId = parentId + (parentId.endsWith(":") ? "" : "/") + name;
                    try {
                        Uri sibling = DocumentsContract.buildDocumentUri(selected.getAuthority(), siblingId);
                        try (InputStream test = getContentResolver().openInputStream(sibling)) {
                            if (test != null && test.read() >= 0) return sibling;
                        }
                    } catch (IOException | SecurityException | IllegalArgumentException exception) {
                        // Android has not granted access to this sibling.
                    }
                }
            }
        }
        return null;
    }

    private void openArchivePair(File primary, File companion) {
        String name = primary.getName().toLowerCase(Locale.ROOT);
        String otherName = companion == null ? "" : companion.getName().toLowerCase(Locale.ROOT);
        if (companion != null && name.endsWith(".img") && otherName.endsWith(".lvz")) {
            archiveImported(companion.getAbsolutePath(), primary.getAbsolutePath());
        } else if (companion != null && name.endsWith(".img") && otherName.endsWith(".dir")) {
            archiveImported(companion.getAbsolutePath(), primary.getAbsolutePath());
        } else {
            archiveImported(primary.getAbsolutePath(), companion == null ? "" : companion.getAbsolutePath());
        }
    }

    private void showMissingArchive(String filename) {
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".img") && lower.startsWith("gta3ps")) {
            finishPendingArchive();
        } else if (lower.endsWith(".lvz")) {
            String expected = StorylandCompanionFiles.expectedNames(filename)[0];
            new AlertDialog.Builder(this).setTitle("IMG not found")
                .setMessage("No " + expected + " in this folder.")
                .setPositiveButton("Find IMG", (dialog, which) -> chooseCompanion())
                .setNeutralButton("Find folder", (dialog, which) -> chooseArchiveFolder())
                .setNegativeButton("Cancel", (dialog, which) -> cancelPendingArchive())
                .show();
        } else if (lower.endsWith(".img")) {
            String expected = StorylandCompanionFiles.expectedNames(filename)[0];
            new AlertDialog.Builder(this).setTitle("LVZ not found")
                .setMessage("No " + expected + " in this folder.")
                .setPositiveButton("Find LVZ", (dialog, which) -> chooseCompanion())
                .setNeutralButton("Find folder", (dialog, which) -> chooseArchiveFolder())
                .setNegativeButton("Open IMG", (dialog, which) -> finishPendingArchive())
                .show();
        } else if (lower.endsWith(".dir")) {
            new AlertDialog.Builder(this).setTitle("IMG not found")
                .setMessage("No matching IMG in this folder.")
                .setPositiveButton("Find IMG", (dialog, which) -> chooseCompanion())
                .setNegativeButton("Cancel", (dialog, which) -> cancelPendingArchive())
                .show();
        } else if (lower.endsWith(".dtz") || lower.equals("game.bin")) {
            new AlertDialog.Builder(this).setTitle("IMG file")
                .setMessage("Select gta3PS2.img or gta3PSP.img if you have one, or open the DTZ by itself.")
                .setPositiveButton("Find IMG", (dialog, which) -> chooseCompanion())
                .setNeutralButton("Open DTZ", (dialog, which) -> finishPendingArchive())
                .setNegativeButton("Cancel", (dialog, which) -> cancelPendingArchive())
                .show();
        } else {
            finishPendingArchive();
        }
    }

    private void copyArchiveDocument(Uri document, File destination) throws IOException {
        long written = 0;
        boolean hasActualData = false;
        try (InputStream input = getContentResolver().openInputStream(document);
             FileOutputStream output = new FileOutputStream(destination)) {
            if (input == null) throw new IOException("Android couldn't read this file.");
            byte[] buffer = new byte[262144];
            int count;
            while ((count = input.read(buffer)) != -1) {
                if (cancelled.get() || Thread.currentThread().isInterrupted()) throw new IOException("Cancelled.");
                if (!hasActualData) {
                    for (int index = 0; index < count; index++) {
                        if (buffer[index] != 0) {
                            hasActualData = true;
                            break;
                        }
                    }
                }
                output.write(buffer,0,count);
                written += count;copiedBytes+=count;
                if (written > 4L*1024*1024*1024) throw new IOException("File is too large (4 GiB limit).");
                updateProgress(destination.getName());
            }
            output.getFD().sync();
        }
        if (written == 0) throw new IOException("The selected file is empty.");
        if (!hasActualData) {
            throw new IOException(destination.getName() +
                " contains only zero bytes. Copy the original game file again.");
        }
        long originalSize = -1;
        try (Cursor details = getContentResolver().query(document,
            new String[]{android.provider.OpenableColumns.SIZE}, null, null, null)) {
            if (details != null && details.moveToFirst() && !details.isNull(0))
                originalSize = details.getLong(0);
        } catch (SecurityException | IllegalArgumentException ignored) {
            originalSize = -1;
        }
        if (originalSize >= 0 && written != originalSize)
            throw new IOException("The file didn't copy completely. Try selecting it again.");
    }

    private void importPrimaryArchive(Uri document) {
        importPrimaryArchive(document, null);
    }

    private void importPrimaryArchive(Uri document, Uri companionInFolder) {
        if (!importing.compareAndSet(false, true)) return;
        cancelled.set(false);
        progress = new ProgressDialog(this);
        progress.setTitle("Open file");
        progress.setMessage("Reading file...");
        progress.setCancelable(false);
        progress.setButton(ProgressDialog.BUTTON_NEGATIVE, "Cancel", (dialog, which) -> cancelled.set(true));
        progress.show();
        copiedBytes = 0;
        lastProgress = 0;
        importThread = new Thread(() -> {
            File folder = new File(new File(getFilesDir(), "archives"), UUID.randomUUID().toString());
            try {
                String filename = resourceDisplayName(document);
                if (!isArchiveFile(filename)) throw new IOException("Choose an LVZ, IMG, DTZ, DIR or ZMG.");
                if (!folder.mkdirs()) throw new IOException("Can't create the file folder.");
                File primary = new File(folder, filename);
                copyArchiveDocument(document, primary);
                if (cancelled.get()) throw new IOException("Cancelled.");

                File companion = null;
                Uri nearby = companionInFolder != null ? companionInFolder :
                    findNearbyArchive(document, filename);
                if (nearby != null) {
                    try {
                        String otherName = resourceDisplayName(nearby);
                        if (StorylandCompanionFiles.matches(filename, otherName)) {
                            File candidate = new File(folder, otherName);
                            try {
                                copyArchiveDocument(nearby, candidate);
                                companion = candidate;
                            } catch (IOException | SecurityException exception) {
                                candidate.delete();
                                if (cancelled.get()) throw new IOException("Cancelled.", exception);
                                if (companionInFolder != null)
                                    throw new IOException("Can't open " + otherName + ": " + exception.getMessage(), exception);
                            }
                        }
                    } catch (IOException | SecurityException exception) {
                        if (cancelled.get()) throw new IOException("Cancelled.", exception);
                        if (companionInFolder != null)
                            throw new IOException(exception.getMessage(), exception);
                    }
                }
                if (cancelled.get()) throw new IOException("Cancelled.");

                final File resolvedCompanion = companion;
                runOnUiThread(() -> {
                    finishImport();
                    if (resolvedCompanion != null) {
                        pendingArchive = null;
                        pendingArchiveFolder = null;
                        pendingArchiveName = null;
                        openArchivePair(primary, resolvedCompanion);
                    } else {
                        pendingArchive = primary;
                        pendingArchiveFolder = folder;
                        pendingArchiveName = filename;
                        showMissingArchive(filename);
                    }
                });
            } catch (Exception exception) {
                removeTree(folder);
                String reason = exception.getMessage() == null ? exception.toString() : exception.getMessage();
                runOnUiThread(() -> { finishImport(); message(reason); });
            }
        }, "Storyland archive import");
        importThread.start();
    }

    private void importCompanionArchive(Uri document) {
        if (pendingArchive==null || pendingArchiveFolder==null || !importing.compareAndSet(false,true)) return;
        cancelled.set(false);
        progress = new ProgressDialog(this);
        progress.setTitle("Open file");
        progress.setMessage("Reading file...");
        progress.setCancelable(false);
        progress.setButton(ProgressDialog.BUTTON_NEGATIVE,"Cancel",(dialog,which)->cancelled.set(true));
        progress.show();
        final File folder = pendingArchiveFolder;
        importThread = new Thread(() -> {
            try {
                String filename=resourceDisplayName(document);
                String ext=filename.toLowerCase(Locale.ROOT);
                String first=pendingArchiveName.toLowerCase(Locale.ROOT);
                if(first.endsWith(".lvz") && !ext.endsWith(".img")) throw new IOException("Choose the matching IMG file.");
                if(first.endsWith(".img") && !ext.endsWith(".lvz") && !ext.endsWith(".dir") &&
                   !ext.endsWith(".dtz") && !ext.endsWith(".bin"))
                    throw new IOException("Choose an LVZ, DIR or GAME.DTZ file.");
                if(first.endsWith(".dir") && !ext.endsWith(".img")) throw new IOException("Choose the matching IMG file.");
                if((first.endsWith(".dtz") || first.equals("game.bin")) && !ext.endsWith(".img")) throw new IOException("Choose gta3PS2.img or gta3PSP.img.");
                String pairedName = StorylandCompanionFiles.localName(pendingArchiveName, filename);
                final File companion = new File(folder, pairedName);
                if (companion.equals(pendingArchive)) throw new IOException("Choose a different file.");
                copyArchiveDocument(document, companion);
                runOnUiThread(() -> {
                    finishImport();
                    openArchivePair(pendingArchive, companion);
                    pendingArchive = null;
                    pendingArchiveFolder = null;
                    pendingArchiveName = null;
                });
            } catch (Exception exception) {
                String reason=exception.getMessage();
                runOnUiThread(() -> {finishImport();message(reason);});
            }
        },"Storyland companion import");
        importThread.start();
    }

    private void finishPendingArchive() {
        if (pendingArchive != null) openArchivePair(pendingArchive, null);
        pendingArchive=null;pendingArchiveFolder=null;pendingArchiveName=null;
    }

    private void cancelPendingArchive() {
        if (pendingArchiveFolder!=null) removeTree(pendingArchiveFolder);
        pendingArchive=null;pendingArchiveFolder=null;pendingArchiveName=null;
    }

    @Override protected String[] getArguments() {
        File files = getFilesDir();
        try {
            File scripts = new File(files, "sannybuilder");
            if (!scripts.isDirectory() && !scripts.mkdirs()) throw new IOException("Cannot create script-definition folder.");
            try (InputStream input = getAssets().open("StorylandUI.ttf");
                 FileOutputStream output = new FileOutputStream(new File(files,"StorylandUI.ttf"))) {
                byte[] buffer = new byte[65536]; int count;
                while ((count=input.read(buffer)) != -1) output.write(buffer,0,count);
            } catch (java.io.FileNotFoundException missingOptionalFont) {
                // The Android viewer also supports the built-in ImGui font.
            }
            for (String name : new String[]{"LCSSCM.INI", "VCSSCM.INI"}) {
                try (InputStream input = getAssets().open("sannybuilder/" + name);
                     FileOutputStream output = new FileOutputStream(new File(scripts, name))) {
                    byte[] buffer = new byte[65536];
                    int count;
                    while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
                }
            }
        } catch (IOException exception) {
            runOnUiThread(() -> message(exception.getMessage()));
        }
        String game = getPreferences(MODE_PRIVATE).getString("game", "");
        if (!game.isEmpty() && new File(game).isDirectory()) return new String[]{files.getAbsolutePath(), game};
        return new String[]{files.getAbsolutePath()};
    }

    public void chooseGame() {
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed() || !importing.compareAndSet(false, true)) return;
            cancelled.set(false);
            lastProgress = 0;
            showGameProgress("Installed games", "Reading installed game folders...");
            importThread = new Thread(() -> {
                try {
                    GameCatalog catalog = installedGames();
                    ArrayList<InstalledGame> games = catalog.games;
                    long available = availableGameStorage();
                    runOnUiThread(() -> {
                        finishImport();
                        if (isFinishing() || isDestroyed() || cancelled.get()) return;
                        if (games.isEmpty() && catalog.interrupted.isEmpty()) {
                            chooseGameFolder();
                            return;
                        }
                        String selected = getPreferences(MODE_PRIVATE).getString("game", "");
                        String[] names = new String[games.size() + (catalog.interrupted.isEmpty() ? 0 : 1)];
                        for (int index = 0; index < games.size(); ++index) {
                            InstalledGame game = games.get(index);
                            names[index] = game.label + " — " + StorylandGameImport.sizeText(game.bytes) +
                                (game.directory.getAbsolutePath().equals(selected) ? " (current)" : "") +
                                "\n" + DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)
                                    .format(new Date(game.directory.lastModified()));
                        }
                        if (!catalog.interrupted.isEmpty())
                            names[games.size()] = "Remove interrupted imports — " +
                                StorylandGameImport.sizeText(catalog.interruptedBytes) + " of files\n" +
                                catalog.interrupted.size() + " incomplete game copies";
                        new AlertDialog.Builder(this)
                            .setTitle("Open game — " + StorylandGameImport.sizeText(available) + " free")
                            .setItems(names, (dialog, index) -> {
                                if (index < games.size()) selectInstalledGame(games.get(index).directory);
                                else removeInterruptedImports(catalog.interrupted);
                            })
                            .setNeutralButton("Import game folder", (dialog, which) -> chooseGameFolder())
                            .setNegativeButton("Cancel", null).show();
                    });
                } catch (Exception exception) {
                    String reason = importFailure(exception);
                    runOnUiThread(() -> { finishImport(); message(reason); });
                }
            }, "Storyland installed games");
            importThread.start();
        });
    }

    private static final class InstalledGame {
        final File directory;
        final String label;
        final long bytes;

        InstalledGame(File directory, String label, long bytes) {
            this.directory = directory;
            this.label = label;
            this.bytes = bytes;
        }
    }

    private static final class GameCatalog {
        final ArrayList<InstalledGame> games = new ArrayList<>();
        final ArrayList<File> interrupted = new ArrayList<>();
        long interruptedBytes;
    }

    private GameCatalog installedGames() throws IOException {
        GameCatalog result = new GameCatalog();
        File folder = new File(getFilesDir(), "games").getCanonicalFile();
        if (!folder.exists()) return result;
        File[] entries = folder.listFiles();
        if (entries == null) throw new IOException("Cannot read the installed game folders.");
        for (File entry : entries) {
            checkImportCancellation();
            if (!entry.isDirectory()) continue;
            if (!entry.getCanonicalFile().equals(entry.getAbsoluteFile()) ||
                !entry.getCanonicalFile().getParentFile().equals(folder.getCanonicalFile())) continue;
            if (StorylandGameImport.isPartialName(entry.getName())) {
                try {
                    StorylandGameImport.Installed partial = StorylandGameImport.inspectPartial(
                        folder, entry, this::checkImportCancellation);
                    result.interrupted.add(entry);
                    result.interruptedBytes = StorylandGameImport.add(result.interruptedBytes, partial.bytes);
                } catch (IOException exception) {
                    checkImportCancellation();
                }
                continue;
            }
            StorylandGameImport.Installed inspected;
            try {
                inspected = StorylandGameImport.inspectInstalled(entry, this::checkImportCancellation);
            } catch (IOException exception) {
                checkImportCancellation();
                continue;
            }
            if (inspected == null) continue;
            String label = getPreferences(MODE_PRIVATE).getString("game_import_label_" + entry.getName(),
                "Imported game " + entry.getName().substring(0, Math.min(8, entry.getName().length())));
            result.games.add(new InstalledGame(entry, label, inspected.bytes));
            updateGameProgress("Reading installed games...\n" + label);
        }
        Collections.sort(result.games, (first, second) -> first.label.compareToIgnoreCase(second.label));
        return result;
    }

    private void removeInterruptedImports(List<File> partials) {
        if (isFinishing() || isDestroyed() || !importing.compareAndSet(false, true)) return;
        cancelled.set(false);
        lastProgress = 0;
        showGameProgress("Remove interrupted imports", "Removing incomplete game copies...");
        importThread = new Thread(() -> {
            try {
                File games = new File(getFilesDir(), "games");
                for (File partial : partials) {
                    checkImportCancellation();
                    StorylandGameImport.removePartial(games, partial, this::checkImportCancellation);
                }
                runOnUiThread(() -> {
                    finishImport();
                    if (!isFinishing() && !isDestroyed() && !cancelled.get()) chooseGame();
                });
            } catch (Exception exception) {
                String reason = importFailure(exception);
                runOnUiThread(() -> { finishImport(); message(reason); });
            }
        }, "Storyland interrupted imports");
        importThread.start();
    }

    private void chooseGameFolder() {
        if (importing.get() || isFinishing() || isDestroyed()) return;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try { startActivityForResult(intent, OPEN_GAME); }
        catch (android.content.ActivityNotFoundException exception) { message("No Android folder picker is installed."); }
    }

    private void selectInstalledGame(File directory) {
        if (isFinishing() || isDestroyed()) return;
        if (!directory.isDirectory()) {
            message("That installed game folder is no longer available.");
            return;
        }
        getPreferences(MODE_PRIVATE).edit().putString("game", directory.getAbsolutePath()).apply();
        openGame(directory.getAbsolutePath());
    }

    private void showGameProgress(String title, String text) {
        progress = new ProgressDialog(this);
        progress.setTitle(title);
        progress.setMessage(text);
        progress.setCancelable(false);
        progress.setButton(ProgressDialog.BUTTON_NEGATIVE, "Cancel", (dialog, which) -> cancelled.set(true));
        progress.show();
    }

    public void exportFiles(String directory) {
        runOnUiThread(() -> {
            if (importing.get()) { message("Finish the current file transfer before exporting."); return; }
            exportDirectory = new File(directory);
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            try { startActivityForResult(intent, EXPORT_FILES); }
            catch (android.content.ActivityNotFoundException exception) { message("No Android folder picker is installed."); }
        });
    }

    private void exportTo(Uri tree) {
        if (exportDirectory == null || !importing.compareAndSet(false, true)) return;
        File source = exportDirectory;
        cancelled.set(false);
        progress = new ProgressDialog(this);
        progress.setTitle("Export Storyland files");
        progress.setMessage("Writing rebuilt files...");
        progress.setCancelable(false);
        progress.setButton(ProgressDialog.BUTTON_NEGATIVE, "Cancel", (dialog, which) -> cancelled.set(true));
        progress.show();
        importThread = new Thread(() -> {
            Uri created = null;
            try {
                String allowed = new File(getFilesDir(), "exports").getCanonicalPath() + File.separator;
                if (!source.getCanonicalPath().startsWith(allowed)) throw new IOException("Invalid export source.");
                Uri parent = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree));
                created = DocumentsContract.createDocument(getContentResolver(), parent, DocumentsContract.Document.MIME_TYPE_DIR, source.getName());
                if (created == null) throw new IOException("Could not create the export folder.");
                File[] files = source.listFiles();
                if (files == null || files.length == 0) throw new IOException("No rebuilt files to export.");
                byte[] buffer = new byte[262144];
                for (File file : files) {
                    if (!file.isFile()) continue;
                    Uri document = DocumentsContract.createDocument(getContentResolver(), created, "application/octet-stream", file.getName());
                    if (document == null) throw new IOException("Could not create " + file.getName());
                    long written = 0;
                    try (InputStream input = new FileInputStream(file);
                         OutputStream output = getContentResolver().openOutputStream(document, "w")) {
                        if (output == null) throw new IOException("Could not write " + file.getName());
                        int count;
                        while ((count = input.read(buffer)) != -1) {
                            if (cancelled.get() || Thread.currentThread().isInterrupted()) throw new IOException("Export cancelled.");
                            output.write(buffer, 0, count); written += count;
                        }
                        output.flush();
                    }
                    if (written != file.length()) throw new IOException("Incomplete export: " + file.getName());
                }
                runOnUiThread(() -> { finishImport(); message("Export complete: " + source.getName()); });
            } catch (Exception exception) {
                if (created != null) try { DocumentsContract.deleteDocument(getContentResolver(), created); } catch (Exception ignored) { }
                String reason = exception.getMessage();
                runOnUiThread(() -> { finishImport(); message("Export failed: " + reason); });
            }
        }, "Storyland export");
        importThread.start();
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == REPLACE_RESOURCE) {
            if (result == RESULT_OK && data != null && data.getData() != null) {
                final Uri document=data.getData();
                if (replacementToken==-1) {
                    try {
                        if (isArchiveFile(resourceDisplayName(document))) importPrimaryArchive(document);
                        else importResource(document);
                    } catch (IOException exception) {message(exception.getMessage());}
                } else importResource(document);
            }
            return;
        }
        if (request == BROWSE_RESOURCE_FOLDER) {
            if (result != RESULT_OK || data == null || data.getData() == null) return;
            Uri folder = data.getData();
            try {
                try {
                    getContentResolver().takePersistableUriPermission(
                        folder, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (SecurityException ignored) {
                    // The folder can still be read until Android revokes this grant.
                }
                getPreferences(MODE_PRIVATE).edit()
                    .putString("archive_folder_uri", folder.toString()).apply();
                browseResourceFolder(folder);
            } catch (RuntimeException exception) {
                message("Android couldn't open that folder.");
            }
            return;
        }
        if (request == OPEN_ARCHIVE_FOLDER) {
            if (pendingArchive == null || pendingArchiveName == null) return;
            if (result != RESULT_OK || data == null || data.getData() == null) return;
            Uri tree = data.getData();
            try {
                if ((data.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION) != 0) {
                    try {
                        getContentResolver().takePersistableUriPermission(
                            tree, Intent.FLAG_GRANT_READ_URI_PERMISSION);
                    } catch (SecurityException ignored) {
                    }
                }
                Uri companion = findInArchiveTree(tree, pendingArchiveName, null);
                if (companion != null) {
                    getPreferences(MODE_PRIVATE).edit()
                        .putString("archive_folder_uri", tree.toString()).apply();
                    importCompanionArchive(companion);
                    return;
                }
                message("Matching file not found in that folder.");
            } catch (SecurityException | IllegalArgumentException exception) {
                message("Can't read that folder.");
            }
            chooseCompanion();
            return;
        }
        if (request == OPEN_ARCHIVE_COMPANION) {
            if (result == RESULT_OK && data != null && data.getData() != null) importCompanionArchive(data.getData());
            else if (pendingArchive!=null) {
                String name=pendingArchiveName.toLowerCase(Locale.ROOT);
                if (name.endsWith(".img") || name.endsWith(".dtz")) finishPendingArchive();
                else cancelPendingArchive();
            }
            return;
        }
        if (request == EXPORT_FILES) {
            if (result == RESULT_OK && data != null && data.getData() != null) exportTo(data.getData());
            return;
        }
        if (request != OPEN_GAME || result != RESULT_OK || data == null || data.getData() == null) return;
        startGameImport(data.getData(), false);
    }

    private void startGameImport(Uri tree, boolean separateCopy) {
        if (isFinishing() || isDestroyed() || !importing.compareAndSet(false, true)) return;
        cancelled.set(false);
        copiedBytes = 0;
        lastProgress = 0;
        showGameProgress("Import game files", "Checking installed games and measuring the selected folder...");
        importThread = new Thread(() -> importGame(tree, separateCopy), "Storyland game import");
        importThread.start();
    }

    private String importSourceKey(Uri tree) throws IOException {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256")
                .digest(tree.toString().getBytes(StandardCharsets.UTF_8));
            StringBuilder key = new StringBuilder("game_import_source_");
            for (byte value : digest) key.append(String.format(Locale.ROOT, "%02x", value & 255));
            return key.toString();
        } catch (NoSuchAlgorithmException exception) {
            throw new IOException("Android cannot identify this game source.", exception);
        }
    }

    private String gameSourceLabel(Uri tree) {
        Uri root = DocumentsContract.buildDocumentUriUsingTree(tree, DocumentsContract.getTreeDocumentId(tree));
        String[] fields = {DocumentsContract.Document.COLUMN_DISPLAY_NAME};
        try (Cursor cursor = getContentResolver().query(root, fields, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                String name = cursor.getString(0);
                if (name != null && !name.trim().isEmpty()) return name;
            }
        } catch (RuntimeException ignored) { }
        return "Imported game";
    }

    private StorylandGameImport.Source gameSource(Uri tree) {
        return new StorylandGameImport.Source() {
            @Override public List<StorylandGameImport.Entry> children(String parent) throws IOException {
                Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parent);
                String[] fields = {DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                    DocumentsContract.Document.COLUMN_MIME_TYPE, DocumentsContract.Document.COLUMN_SIZE};
                ArrayList<StorylandGameImport.Entry> entries = new ArrayList<>();
                try (Cursor cursor = getContentResolver().query(children, fields, null, null, null)) {
                    if (cursor == null) throw new IOException("Android could not list this game folder.");
                    while (cursor.moveToNext()) {
                        checkImportCancellation();
                        if (entries.size() >= StorylandGameImport.MAX_ENTRIES)
                            throw new IOException("The selected folder contains too many files or folders.");
                        entries.add(new StorylandGameImport.Entry(cursor.getString(0), cursor.getString(1),
                            DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2)),
                            cursor.isNull(3) ? -1 : cursor.getLong(3)));
                    }
                } catch (SecurityException exception) {
                    throw new IOException("Android denied access to the selected folder.", exception);
                }
                return entries;
            }

            @Override public InputStream open(String documentId) throws IOException {
                checkImportCancellation();
                try {
                    return getContentResolver().openInputStream(
                        DocumentsContract.buildDocumentUriUsingTree(tree, documentId));
                } catch (SecurityException exception) {
                    throw new IOException("Android denied access to a game file.", exception);
                }
            }
        };
    }

    private void checkImportCancellation() throws IOException {
        if (cancelled.get() || Thread.currentThread().isInterrupted()) throw new IOException("Cancelled.");
    }

    private long availableGameStorage() throws IOException {
        try {
            return new StatFs(getFilesDir().getAbsolutePath()).getAvailableBytes();
        } catch (IllegalArgumentException exception) {
            throw new IOException("Android could not measure available app storage.", exception);
        }
    }

    private String importFailure(Exception exception) {
        return exception.getMessage() == null ? exception.toString() : exception.getMessage();
    }

    private void importGame(Uri tree, boolean separateCopy) {
        File games = new File(getFilesDir(), "games");
        String identifier = UUID.randomUUID().toString();
        File staging = new File(games, ".import-" + identifier);
        File destination = new File(games, identifier);
        boolean created = false;
        try {
            String sourceKey = importSourceKey(tree);
            String label = gameSourceLabel(tree);
            String existingPath = getPreferences(MODE_PRIVATE).getString(sourceKey, "");
            if (!separateCopy && !existingPath.isEmpty()) {
                File existing = new File(existingPath).getCanonicalFile();
                boolean available = false;
                if (existing.isDirectory() && games.getCanonicalFile().equals(existing.getParentFile()) &&
                    !existing.getName().startsWith(".import-")) {
                    try {
                        available = StorylandGameImport.inspectInstalled(existing, this::checkImportCancellation) != null;
                    } catch (IOException exception) {
                        checkImportCancellation();
                    }
                }
                if (available) {
                    runOnUiThread(() -> {
                        finishImport();
                        if (isFinishing() || isDestroyed() || cancelled.get()) return;
                        new AlertDialog.Builder(this).setTitle("Game already installed")
                            .setMessage(label + " is already imported. Opening it uses the installed files. " +
                                "A separate copy needs the full game's storage again.")
                            .setPositiveButton("Open installed game", (dialog, which) -> selectInstalledGame(existing))
                            .setNeutralButton("Import separate copy", (dialog, which) -> startGameImport(tree, true))
                            .setNegativeButton("Cancel", null).show();
                    });
                    return;
                }
            }
            StorylandGameImport.Source source = gameSource(tree);
            long blockBytes = new StatFs(getFilesDir().getAbsolutePath()).getBlockSizeLong();
            StorylandGameImport.Progress listener = new StorylandGameImport.Progress() {
                @Override public void scanned(String name, long bytes, int files, boolean measuring) {
                    updateGameProgress((measuring ? "Measuring an unreported file size...\n" : "Checking game files...\n") +
                        name + "\n" + files + " files, " + StorylandGameImport.sizeText(bytes) + " measured");
                }

                @Override public void copied(String name, long bytes, long totalBytes, int files, int totalFiles) {
                    updateGameProgress(name + "\n" + StorylandGameImport.sizeText(bytes) + " / " +
                        StorylandGameImport.sizeText(totalBytes) + " imported\n" + files + " / " + totalFiles + " files");
                }
            };
            StorylandGameImport.Plan plan = StorylandGameImport.plan(source,
                DocumentsContract.getTreeDocumentId(tree), blockBytes, this::checkImportCancellation, listener);
            long available = availableGameStorage();
            StorylandGameImport.requireSpace(plan, available);
            String budget = StorylandGameImport.spaceDescription(plan, available);
            runOnUiThread(() -> { if (progress != null) progress.setMessage(budget + "\n\nImporting game files..."); });
            checkImportCancellation();
            if (!games.isDirectory() && !games.mkdirs()) throw new IOException("Cannot create the installed-games folder.");
            if (destination.exists() || !staging.mkdir()) throw new IOException("Cannot create a new game import folder.");
            created = true;
            StorylandGameImport.copy(plan, source, staging, this::availableGameStorage,
                this::checkImportCancellation, listener);
            checkImportCancellation();
            if (!staging.renameTo(destination)) throw new IOException("Cannot finish installing the game folder.");
            created = false;
            getPreferences(MODE_PRIVATE).edit().putString(sourceKey, destination.getAbsolutePath())
                .putString("game_import_label_" + identifier, label).apply();
            runOnUiThread(() -> {
                finishImport();
                if (!cancelled.get()) selectInstalledGame(destination);
            });
        } catch (Exception exception) {
            String reason = importFailure(exception);
            if (created) {
                try { StorylandGameImport.removePartial(games, staging); }
                catch (IOException cleanup) { reason += "\nThe incomplete copy could not be removed: " + cleanup.getMessage(); }
            }
            final String failure = reason;
            runOnUiThread(() -> { finishImport(); message(failure); });
        }
    }

    private void updateGameProgress(String text) {
        long now = android.os.SystemClock.uptimeMillis();
        if (now - lastProgress < 200) return;
        lastProgress = now;
        runOnUiThread(() -> { if (progress != null) progress.setMessage(text); });
    }

    private void updateProgress(String name) {
        long now = android.os.SystemClock.uptimeMillis();
        if (now - lastProgress < 200) return;
        lastProgress = now;
        String text = name + "\n" + (copiedBytes / (1024 * 1024)) + " MiB imported";
        runOnUiThread(() -> { if (progress != null) progress.setMessage(text); });
    }

    private void removeTree(File file) {
        File[] children = file.listFiles();
        if (children != null) for (File child : children) removeTree(child);
        file.delete();
    }

    private void finishImport() {
        importing.set(false);
        if (progress != null) { progress.dismiss(); progress = null; }
    }

    private void message(String text) {
        if (!isFinishing() && !isDestroyed())
            new AlertDialog.Builder(this).setTitle("Storyland").setMessage(text).setPositiveButton("OK", null).show();
    }

    @Override protected void onResume() {
        super.onResume();
        if (programMusic != null) programMusic.setForeground(true);
    }

    @Override protected void onPause() {
        if (programMusic != null) programMusic.setForeground(false);
        super.onPause();
    }

    @Override protected void onDestroy() {
        if (programMusic != null) programMusic.stop();
        cancelled.set(true);
        if (importThread != null) importThread.interrupt();
        finishImport();
        super.onDestroy();
    }
}
