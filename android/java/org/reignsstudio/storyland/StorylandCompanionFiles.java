package org.reignsstudio.storyland;

import java.util.Locale;

final class StorylandCompanionFiles {
    private StorylandCompanionFiles() {}

    static String[] expectedNames(String filename) {
        int extensionAt = filename.lastIndexOf('.');
        if (extensionAt <= 0) return new String[0];
        String stem = filename.substring(0, extensionAt);
        String lower = filename.toLowerCase(Locale.ROOT);
        if (lower.endsWith(".lvz") || lower.endsWith(".dir")) {
            return new String[]{stem + ".img"};
        }
        if (lower.endsWith(".dtz") || lower.equals("game.bin")) {
            return new String[]{"gta3PS2.img", "gta3PSP.img"};
        }
        if (lower.endsWith(".img")) {
            if (lower.startsWith("gta3ps")) {
                return new String[]{"GAME.DTZ", "GAME.BIN", stem + ".dir"};
            }
            return new String[]{stem + ".lvz", stem + ".dir"};
        }
        return new String[0];
    }

    static boolean matches(String filename, String companion) {
        for (String name : expectedNames(filename)) {
            if (name.equalsIgnoreCase(companion)) return true;
        }
        return false;
    }

    static String localName(String filename, String companion) {
        if (matches(filename, companion)) return companion;
        int extensionAt = companion.lastIndexOf('.');
        if (extensionAt < 0) return companion;
        String extension = companion.substring(extensionAt);
        for (String name : expectedNames(filename)) {
            if (name.toLowerCase(Locale.ROOT).endsWith(extension.toLowerCase(Locale.ROOT))) return name;
        }
        return companion;
    }
}
