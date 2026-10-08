// SPDX-License-Identifier: GPL-2.0-or-later
// Photos mode - inline video playback in the viewer (Qt Multimedia, FFmpeg
// backend). Loaded through a Loader: without the Qt Multimedia QML module the
// viewer falls back to the system player.

import QtQuick
import QtMultimedia

Item {
    id: player

    property url  source
    property bool autoPlay: true

    readonly property bool playing:  media.playbackState === MediaPlayer.PlayingState
    readonly property int  duration: media.duration
    readonly property int  position: media.position
    readonly property bool started:  media.position > 0 || playing
    readonly property bool failed:   media.error !== MediaPlayer.NoError
    property alias         muted:    audio.muted

    function toggle() {
        if (playing)
            media.pause()
        else {
            if ((media.duration > 0) && (media.position >= media.duration - 50))
                media.position = 0

            media.play()
        }
    }

    function seek(ms) {
        media.position = Math.max(0, Math.min(media.duration, ms))
    }

    function stop() {
        media.stop()
    }

    onSourceChanged: {
        media.stop()
        media.source = source

        if (autoPlay && (source.toString().length > 0))
            media.play()
    }

    MediaPlayer {
        id: media
        videoOutput: output
        audioOutput: audio
    }

    AudioOutput {
        id: audio
    }

    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode:     VideoOutput.PreserveAspectFit

        // Keep the poster frame (thumbnail) visible until the first frame.
        opacity:      player.started ? 1.0 : 0.0
    }
}
