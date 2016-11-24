using UnityEngine;
using UnityEngine.UI;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using System.Linq;

public class BeatTracker : MonoBehaviour {

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static void Initialize(int sampling_rate);

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static float GetBPM();

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static void UpdateAnalyzer();

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static float GetBPMVU(int index);

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static float GetBPMScore(int index);

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static float GetRMS(int index);

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static int GetWindowSize();

	// Use this for initialization
	void Start () {
		// Initializeは複数回呼んでもかまわない
		Initialize(AudioSettings.outputSampleRate);
	}
	
	// Update is called once per frame
	void Update () {
		// RMS, BPMなどの取得にはUpdateAnalyzer()が必要
		UpdateAnalyzer();
#if true
		// BPM解析はまだ実験中。
		// BPMスコアの順位を取得
		// - スコアをDictionary Listへ
		var score = new Dictionary<int, float>();
		for (var i = 0; i < 180 - 45; ++i) {
			score[i] = GetBPMScore(i);
		}

		var sorted = score.OrderByDescending(x => x.Value);
		float max = sorted.Select(x => x.Value).First();
		float min = sorted.Select(x => x.Value).Last();
		int max_index = sorted.Select(x => x.Key).First();

		// 順位を表示
		GetComponent<Text>().text = "top: " + GetBPM() + "\n";
		GetComponent<Text>().color = Color.black;
		for (var i = 0; i < 7; ++i) {
			int index = sorted.Select(x => x.Key).ElementAt(i);
			GetComponent<Text>().text += (i + 1) + ": " + 11250.0 / (index + 45) + "\n";
		}

		// Draw BPM Score
		for (var i = 0; i < 180 - 45 + 1; ++i) {
			if (max - min < 0.00001f) {
				Debug.DrawLine(
					new Vector3(i / 5.0f, 0.0f, 0),
					new Vector3((i + 1) / 5.0f, 0.0f, 0),
					Color.red);
			} else {
				var color = Color.red;
				// スコアが7位以内なら色付け
				if (i == 93 - 45) {
					color = Color.green;
					GetComponent<Text>().text += GetBPMScore(i) + "\n";
				}
				if (sorted.Select(x => x.Key).Take(7).ToList().Exists(x => x == i)) {
					color = Color.cyan;
				}
				if (i == max_index) {
					color = Color.yellow;
					GetComponent<Text>().text += GetBPMScore(max_index) + "\n";
				}

				Debug.DrawLine(
					new Vector3(i / 5.0f, (GetBPMScore(i) - min) / (max - min), 0),
					new Vector3(i / 5.0f, 0, 0),
					color);
			}
		}

		var window_size = GetWindowSize();
		for (var i = 0; i < window_size - 1; ++i) {
			// Draw VU amplitude history
			Debug.DrawLine(
				new Vector3(i / 120.0f, GetBPMVU(window_size - 1 - i) * 10.0f + 2.0f, 0),
				new Vector3((i + 1) / 120.0f, GetBPMVU(window_size - 1 - (i + 1)) * 10.0f + 2.0f, 0),
				Color.green);

			// Draw RMS
			Debug.DrawLine(
				new Vector3(i / 120.0f, GetRMS(window_size - 1 - i) * 10.0f + 6.0f, 0),
				new Vector3((i + 1) / 120.0f, GetRMS(window_size - 1 - (i + 1)) * 10.0f + 6.0f, 0),
				Color.magenta);
		}
#endif
	}
}
