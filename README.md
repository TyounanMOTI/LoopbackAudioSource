# LoopbackAudioSource
Windowsで再生している音声をAudioSourceにループバックするためのUnity Native Pluginです。

Unity Native Plugin to loopback windows audio to AudioSource.

# 使い方 Usage
1. LoopbackAudioSource/scripts/LoopbackAudioSource.cs をAudioSourceのあるGameObjectにコンポーネントして追加する。

    Add LoopbackAudioSource/scripts/LoopbackAudioSource.cs to GameObject with Unity AudioSource as Component.

2. インスペクタで、Channelに0（左チャンネル）または 1（右チャンネル）を入れる

    Set 0 (left channel) or 1 (right channel) to Channel at Inspector.

3. Audioのプロジェクト設定で、Spatializer Pluginを Oculus Spatializer + Loopback に設定する

    Set Spatializer Plugin at Audio Project Setting to "Oculus Spatializer + Loopback".

# 仕組み Mechanism
Oculus Spatializerの立体音響処理の前に、WASAPIで録音したループバック音声を流し込んでいます。

Hook Oculus Spatializer and sink recorded loopback audio by WASAPI.

# サンプル Sample
build/unity/sandbox に、OculusAudioとの連携サンプルプロジェクトがあります。

Oculus Audio interoperation sample at build/unity/sandbox

# License
MIT License

# Acknowledgement
- Oculus Audio SDK Unity Plugin: Copyright (c) 2015 Oculus VR, LLC.
- Unity Native Audio Plugin SDK: Copyright (c) 2014, Unity Technologies.

# 既知の問題 Known Issue
数分ごとに音途切れが発生することがあります。修正調査中です。

Small glitch in few minutes interval. This will be fixed.
