#include "livekit_room_widget.h"

#include <QVBoxLayout>
#include <QUrl>

LiveKitRoomWidget::LiveKitRoomWidget(const QString &url, const QString &token, const QString &roomLabel, QWidget *parent)
    : QWidget(parent), roomTitle(roomLabel.isEmpty() ? QStringLiteral("Room") : roomLabel) {
    auto *layout = new QVBoxLayout(this);
    webView = new QWebEngineView(this);
    layout->addWidget(webView);

    const QString html = buildHtml(url, token, roomTitle);
    webView->setHtml(html, QUrl("https://cdn.livekit.io"));
}

QString LiveKitRoomWidget::escapeForJs(const QString &value) const {
    QString escaped = value;
    escaped.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
    escaped.replace(QStringLiteral("'"), QStringLiteral("\\'"));
    escaped.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
    escaped.replace(QStringLiteral("\r"), QStringLiteral("\\r"));
    return escaped;
}

QString LiveKitRoomWidget::buildHtml(const QString &url, const QString &token, const QString &roomLabel) const {
    const QString urlJs = escapeForJs(url);
    const QString tokenJs = escapeForJs(token);
    const QString roomLabelJs = escapeForJs(roomLabel);

    const QString html = QString(R"(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>LiveKit Desktop Client</title>
  <script src="https://cdn.livekit.io/js/1.15.7/livekit-client.min.js"></script>
  <style>
    :root {
      color-scheme: dark;
    }
    body { margin: 0; font-family: 'Segoe UI', Roboto, sans-serif; background: #0b1622; color: #d9e2ef; }
    #header { padding: 12px 16px; background: #0f2236; display: flex; justify-content: space-between; align-items: center; }
    #header .meta { display: flex; gap: 12px; align-items: center; font-size: 14px; }
    #status { font-weight: 600; }
    #controls { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 8px; padding: 12px; background: #12283c; }
    #controls label { display: flex; flex-direction: column; gap: 4px; font-size: 13px; }
    #controls select, #controls button { padding: 8px; border-radius: 6px; border: 1px solid #1f3b57; background: #0f2236; color: #d9e2ef; }
    #controls button { cursor: pointer; background: #2d8cf0; border: none; }
    #controls button.secondary { background: #1f3b57; }
    #controls button.danger { background: #d14343; }
    #controls button:hover { filter: brightness(1.05); }
    #videos { display: flex; flex-wrap: wrap; gap: 12px; padding: 12px; }
    video { width: 320px; background: #000; border-radius: 8px; }
    #chat { padding: 12px; border-top: 1px solid #1f3b57; background: #0f2236; }
    #chatLog { height: 160px; overflow-y: auto; border: 1px solid #1f3b57; border-radius: 6px; padding: 8px; background: #091420; margin-bottom: 8px; font-size: 13px; }
    #chatInputRow { display: flex; gap: 8px; }
    #chatInput { flex: 1; padding: 8px; border-radius: 6px; border: 1px solid #1f3b57; background: #0b1622; color: #d9e2ef; }
    #chatSend { padding: 8px 12px; border-radius: 6px; border: none; background: #2d8cf0; color: white; cursor: pointer; }
    #logs { padding: 12px; background: #0f2236; height: 160px; overflow: auto; font-size: 12px; border-top: 1px solid #1f3b57; }
  </style>
</head>
<body>
  <div id="header">
    <div class="meta">
      <div>Server: %1</div>
      <div>Room: %2</div>
    </div>
    <div id="status">Starting…</div>
  </div>
  <div id="controls">
    <button id="reconnect">Reconnect</button>
    <button id="muteAudio">Mute audio</button>
    <button id="muteVideo">Mute video</button>
    <button id="screenShare" class="secondary">Share screen</button>
    <label>Microphone
      <select id="micSelect"></select>
    </label>
    <label>Camera
      <select id="camSelect"></select>
    </label>
  </div>
  <div id="videos"></div>
  <div id="chat">
    <div id="chatLog"></div>
    <div id="chatInputRow">
      <input id="chatInput" placeholder="Send a message to the room" />
      <button id="chatSend">Send</button>
    </div>
  </div>
  <div id="logs"></div>
  <script>
    const url = '%3';
    const token = '%4';
    const roomLabel = '%5';
    const logs = document.getElementById('logs');
    const status = document.getElementById('status');
    const videos = document.getElementById('videos');
    const micSelect = document.getElementById('micSelect');
    const camSelect = document.getElementById('camSelect');
    const muteAudioBtn = document.getElementById('muteAudio');
    const muteVideoBtn = document.getElementById('muteVideo');
    const screenShareBtn = document.getElementById('screenShare');
    const reconnectBtn = document.getElementById('reconnect');
    const chatLog = document.getElementById('chatLog');
    const chatInput = document.getElementById('chatInput');
    const chatSend = document.getElementById('chatSend');

    let room;
    let screenSharePub;

    function log(line) {
      const el = document.createElement('div');
      el.textContent = '[' + new Date().toLocaleTimeString() + '] ' + line;
      logs.appendChild(el);
      logs.scrollTop = logs.scrollHeight;
    }

    function logChat(sender, message) {
      const row = document.createElement('div');
      row.textContent = sender + ': ' + message;
      chatLog.appendChild(row);
      chatLog.scrollTop = chatLog.scrollHeight;
    }

    function addVideoElement(track, prepend = false, muted = false) {
      const el = track.attach();
      el.autoplay = true;
      el.playsInline = true;
      el.muted = muted;
      if (prepend) {
        videos.prepend(el);
      } else {
        videos.appendChild(el);
      }
    }

    function attachTrack(publication) {
      publication.on('subscribed', track => {
        log('Track subscribed: ' + track.sid + ' (' + track.kind + ')');
        if (track.kind === 'video') {
          addVideoElement(track, false, false);
        } else if (track.kind === 'audio') {
          track.attach();
        }
      });
    }

    async function populateDevices() {
      const devices = await navigator.mediaDevices.enumerateDevices();
      const mics = devices.filter(d => d.kind === 'audioinput');
      const cams = devices.filter(d => d.kind === 'videoinput');
      micSelect.innerHTML = '';
      camSelect.innerHTML = '';
      mics.forEach(d => {
        const opt = document.createElement('option');
        opt.value = d.deviceId;
        opt.textContent = d.label || 'Microphone';
        micSelect.appendChild(opt);
      });
      cams.forEach(d => {
        const opt = document.createElement('option');
        opt.value = d.deviceId;
        opt.textContent = d.label || 'Camera';
        camSelect.appendChild(opt);
      });
    }

    async function replaceTrack(kind, deviceId) {
      if (!room) return;
      const constraints = kind === 'audio' ? { audio: { deviceId: { exact: deviceId } }, video: false }
                                           : { audio: false, video: { deviceId: { exact: deviceId } } };
      const tracks = await LiveKit.createLocalTracks(constraints);
      const newTrack = tracks.find(t => t.kind === kind);
      if (!newTrack) return;

      const pubs = kind === 'audio' ? [...room.localParticipant.audioTracks.values()] : [...room.localParticipant.videoTracks.values()];
      pubs.forEach(pub => {
        if (pub.track) {
          room.localParticipant.unpublishTrack(pub.track);
          pub.track.stop();
        }
      });

      await room.localParticipant.publishTrack(newTrack);
      if (kind === 'video') {
        addVideoElement(newTrack, true, true);
      }
      log('Switched ' + kind + ' device');
    }

    async function toggleScreenShare() {
      if (!room) return;
      if (screenSharePub) {
        room.localParticipant.unpublishTrack(screenSharePub.track);
        screenSharePub.track.stop();
        screenSharePub = undefined;
        screenShareBtn.textContent = 'Share screen';
        screenShareBtn.classList.add('secondary');
        log('Stopped screen share');
        return;
      }
      try {
        const stream = await navigator.mediaDevices.getDisplayMedia({ video: true });
        const [track] = stream.getVideoTracks();
        screenSharePub = await room.localParticipant.publishTrack(track);
        addVideoElement(screenSharePub.track, true, true);
        screenShareBtn.textContent = 'Stop share';
        screenShareBtn.classList.remove('secondary');
        screenShareBtn.classList.add('danger');
        log('Screen share started');
        track.addEventListener('ended', () => {
          if (screenSharePub && screenSharePub.track === track) {
            toggleScreenShare();
          }
        });
      } catch (err) {
        log('Screen share failed: ' + err);
      }
    }

    async function connectRoom() {
      if (room) {
        try { await room.disconnect(); } catch (e) {}
      }
      status.textContent = 'Connecting…';
      logs.textContent = '';
      videos.textContent = '';
      chatLog.textContent = '';
      try {
        await populateDevices();
        const audioConstraint = micSelect.value ? { deviceId: { exact: micSelect.value } } : true;
        const videoConstraint = camSelect.value ? { deviceId: { exact: camSelect.value } } : true;
        room = await LiveKit.connect(url, token, { autoSubscribe: true });
        window.room = room;
        status.textContent = 'Connected as ' + room.localParticipant.identity;
        log('Connected to ' + roomLabel);

        const localTracks = await LiveKit.createLocalTracks({ audio: audioConstraint, video: videoConstraint });
        localTracks.forEach(t => {
          room.localParticipant.publishTrack(t);
          if (t.kind === 'video') {
            addVideoElement(t, true, true);
          }
        });

        room.participants.forEach(p => {
          p.tracks.forEach(attachTrack);
          p.on('trackPublished', attachTrack);
        });

        room.on('participantConnected', p => log(p.identity + ' joined'));
        room.on('participantDisconnected', p => log(p.identity + ' left'));
        room.on('disconnected', () => status.textContent = 'Disconnected');
        room.on('dataReceived', (payload, participant, kind, topic) => {
          const decoder = new TextDecoder();
          const msg = decoder.decode(payload);
          logChat((participant && participant.identity) || 'server', msg);
        });

        muteAudioBtn.onclick = () => {
          const pubs = [...room.localParticipant.audioTracks.values()];
          if (pubs.length === 0) return;
          const nextMuted = !pubs.every(p => p.isMuted);
          pubs.forEach(p => p.setMuted(nextMuted));
          muteAudioBtn.textContent = nextMuted ? 'Unmute audio' : 'Mute audio';
        };

        muteVideoBtn.onclick = () => {
          const pubs = [...room.localParticipant.videoTracks.values()];
          if (pubs.length === 0) return;
          const nextMuted = !pubs.every(p => p.isMuted);
          pubs.forEach(p => p.setMuted(nextMuted));
          muteVideoBtn.textContent = nextMuted ? 'Unmute video' : 'Mute video';
        };

        screenShareBtn.onclick = () => toggleScreenShare();

        micSelect.onchange = () => replaceTrack('audio', micSelect.value);
        camSelect.onchange = () => replaceTrack('video', camSelect.value);

        chatSend.onclick = () => {
          if (!room) return;
          const text = chatInput.value.trim();
          if (!text) return;
          const encoder = new TextEncoder();
          room.localParticipant.publishData(encoder.encode(text), { reliable: true });
          logChat(room.localParticipant.identity, text);
          chatInput.value = '';
        };

        chatInput.addEventListener('keyup', (ev) => {
          if (ev.key === 'Enter') chatSend.onclick();
        });
      } catch (err) {
        console.error(err);
        status.textContent = 'Connection failed';
        log('Error: ' + err);
      }
    }

    reconnectBtn.onclick = () => connectRoom();

    connectRoom();
  </script>
</body>
</html>
)").arg(urlJs, roomLabelJs, urlJs, tokenJs, roomLabelJs);

    return html;
}
