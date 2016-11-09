using UnityEngine;
using System.Collections;
using UnityEngine.Audio;

public class LoopbackAudioSource : MonoBehaviour {
	// どのチャンネルの録音音声を取ってくるか
	// 0: 左チャンネル
	// 1: 右チャンネル
	public int Channel = 0;

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
	}

	void SetParameter() {
		var Source = GetComponent<AudioSource>();
		Source.SetSpatializerFloat(5, 1.0f);
		Source.SetSpatializerFloat(6, Channel);
	}
}
