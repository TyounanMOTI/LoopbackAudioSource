using UnityEngine;
using System.Collections;
using UnityEngine.Audio;

public class LoopbackAudioSource : MonoBehaviour {
	// どのチャンネルの録音音声を取ってくるか
	// 0: 左チャンネル
	// 1: 右チャンネル
	public int Channel = 0;

	private float[] spectrum = new float[1024];
	public float[] eightBandLevels = new float[8];

	void Start() {
		SetParameter();

		var clip = AudioClip.Create("Loopback AudioSource", 1, 1, 48000, true);
		var source = GetComponent<AudioSource>();
		source.clip = clip;
		source.Play();
	}

	void Update()
	{
		SetParameter();

		// スペクトラムを8バンドへ分解
		GetComponent<AudioSource>().GetSpectrumData(spectrum, 0, FFTWindow.BlackmanHarris);
		int index = 0;
		for (int i = 0; i < 8; ++i) {
			var nextBand = Mathf.Exp(Mathf.Log(1024) / 8 * (i + 1));
			var weight = 1.0f / (nextBand - index);
			var sum = 0.0f;
			for (; index < nextBand; index++) {
				sum += spectrum[index];
			}
			eightBandLevels[i] = Mathf.Sqrt(weight * sum);
		}
	}

	void SetParameter() {
		var Source = GetComponent<AudioSource>();
		Source.SetSpatializerFloat(5, 1.0f);
		Source.SetSpatializerFloat(6, Channel);
	}
}
