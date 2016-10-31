using UnityEngine;
using System.Collections;
using UnityEngine.Audio;
using System.Runtime.InteropServices;

public class LoopbackAudioSource : MonoBehaviour {
	// WASAPI音声録音のバッファサイズを決定する。
	// 小さくすると音声遅延が軽減されるが、音途切れの可能性が高まる。
	// 変更の反映にはUnityの再起動が必要。
	private int BufferLengthInMillisecond = 32;

	// どのチャンネルの録音音声を取ってくるか
	// 0: 左チャンネル
	// 1: 右チャンネル
	public int Channel = 0;

	[DllImport("LoopbackAudioSource")]
	private static extern int IsInitialized();

	[DllImport("LoopbackAudioSource")]
	private static extern int Initialize(
		int BufferLengthInMillisecond);

	[DllImport("LoopbackAudioSource")]
	private static extern int GetSamplingRate();

	[DllImport("LoopbackAudioSource")]
	private static extern int GetNumChannels();

	[DllImport("LoopbackAudioSource")]
	private static extern void ResetBuffer(int Channel);

	[DllImport("LoopbackAudioSource")]
	private static extern void GetSamples(int Channel, int Length, ref System.IntPtr Recorded);

	void Start () {
		if (IsInitialized() == 0) {
			int result = Initialize(BufferLengthInMillisecond);
			if (result == 0) {
				Debug.LogError("Failed to initialize loopback audio source native plugin.");
				return;
			}
		}
		int SamplingRate = GetSamplingRate();
		AudioClip clip = AudioClip.Create("LoopbackAudioClip", 128, 1, SamplingRate, true, OnAudioRead);
		AudioSource Source = GetComponent<AudioSource>();
		Source.clip = clip;

		ResetBuffer(Channel);
		Source.Play();
	}

	void OnAudioRead (float[] Output) {
		System.IntPtr Recorded = System.IntPtr.Zero;
		GetSamples(Channel, Output.Length, ref Recorded);
		Marshal.Copy(Recorded, Output, 0, Output.Length);
	}
}
