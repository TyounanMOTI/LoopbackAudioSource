# LoopbackAudioSource
Windowsで再生している音声をAudioSourceにループバックするためのUnity Native Pluginです。

Unity Native Plugin to loopback windows audio to AudioSource.

# 使い方 Usage
1. LoopbackAudioSource/scripts/LoopbackAudioSource.cs をAudioSourceのあるGameObjectにコンポーネントして追加する。

    Add LoopbackAudioSource/scripts/LoopbackAudioSource.cs to GameObject with Unity AudioSource as Component.

2. インスペクタで、Channelに0（左チャンネル）または 1（右チャンネル）を入れる

    Set 0 (left channel) or 1 (right channel) to Channel at Inspector.

# 仕組み Mechanism
AudioSourceのAudioClipを上書きして、WASAPIで録音したループバック音声を流し込んでいます。

Overwrite AudioClip in AudioSource and sink recorded loopback audio by WASAPI.

# サンプル Sample
build/unity/sandbox に、OculusAudioとの連携サンプルプロジェクトがあります。

Oculus Audio interoperation sample at build/unity/sandbox

# License
MIT License

# Acknowledgement
Included Oculus Audio SDK Unity Plugin in build/unity/sandbox sample.

# 既知の問題 Known Issue
録音から再生まで１秒の遅延があります。 

There's 1 second delay between recording and playback.

http://answers.unity3d.com/questions/1189718/audioclip-stream-has-delay.html
