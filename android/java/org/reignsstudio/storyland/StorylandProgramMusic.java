package org.reignsstudio.storyland;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.MediaPlayer;
import android.util.Log;
import java.io.IOException;
import java.util.Calendar;

final class StorylandProgramMusic {
    private final Context context;
    private final AudioManager audioManager;
    private final AudioAttributes attributes;
    private final AudioFocusRequest focusRequest;
    private MediaPlayer player;
    private int track;
    private boolean prepared;
    private boolean foreground;
    private boolean focused;
    private boolean focusRequested;
    private volatile String error = "";

    StorylandProgramMusic(Context context) {
        this.context = context;
        audioManager = (AudioManager) context.getSystemService(Context.AUDIO_SERVICE);
        attributes = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
            .build();
        focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
            .setAudioAttributes(attributes)
            .setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(change -> {
                focused = change == AudioManager.AUDIOFOCUS_GAIN;
                if (player == null || !prepared) return;
                try {
                    if (focused && foreground) player.start();
                    else if (player.isPlaying()) player.pause();
                } catch (IllegalStateException exception) {
                    fail("Music playback stopped: " + exception.getMessage());
                }
            }).build();
    }

    String error() { return error; }

    void play(int requestedTrack) {
        if (requestedTrack == track) return;
        stop();
        track = requestedTrack;
        error = "";
        if (track == 0) return;
        Calendar date = Calendar.getInstance();
        if (date.get(Calendar.MONTH) == Calendar.NOVEMBER && date.get(Calendar.DAY_OF_MONTH) == 11) return;
        String asset;
        if (track == 1) asset = "music/reigns_intro.mp3";
        else if (track == 2) asset = "music/special_thanks.mp3";
        else { fail("Unknown interface music track."); return; }
        MediaPlayer opening = new MediaPlayer();
        player = opening;
        try (AssetFileDescriptor file = context.getAssets().openFd(asset)) {
            opening.setAudioAttributes(attributes);
            opening.setDataSource(file.getFileDescriptor(), file.getStartOffset(), file.getLength());
            opening.setLooping(track == 2);
            opening.setOnPreparedListener(ready -> {
                if (ready != player) return;
                prepared = true;
                updatePlayback();
            });
            opening.setOnErrorListener((failed, what, extra) -> {
                if (failed == player) fail("Music decoder error " + what + " / " + extra + ".");
                return true;
            });
            opening.prepareAsync();
        } catch (IOException | RuntimeException exception) {
            fail("Could not play bundled music: " + exception.getMessage());
        }
    }

    void setForeground(boolean value) {
        foreground = value;
        if (!foreground) {
            if (player != null && prepared) {
                try { if (player.isPlaying()) player.pause(); }
                catch (IllegalStateException exception) { fail("Music pause failed: " + exception.getMessage()); }
            }
            abandonFocus();
        } else updatePlayback();
    }

    private void updatePlayback() {
        if (!foreground || player == null || !prepared) return;
        if (!focusRequested && audioManager != null) {
            focusRequested = true;
            focused = audioManager.requestAudioFocus(focusRequest) == AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
        }
        if (audioManager == null) focused = true;
        if (!focused) return;
        try { player.start(); }
        catch (IllegalStateException exception) { fail("Music playback failed: " + exception.getMessage()); }
    }

    private void abandonFocus() {
        if (focusRequested && audioManager != null) audioManager.abandonAudioFocusRequest(focusRequest);
        focusRequested = false;
        focused = false;
    }

    private void fail(String reason) {
        error = reason;
        Log.e("Storyland", reason);
        stop();
    }

    void stop() {
        if (player != null) {
            player.setOnPreparedListener(null);
            player.setOnErrorListener(null);
            player.release();
            player = null;
        }
        prepared = false;
        track = 0;
        abandonFocus();
    }
}
