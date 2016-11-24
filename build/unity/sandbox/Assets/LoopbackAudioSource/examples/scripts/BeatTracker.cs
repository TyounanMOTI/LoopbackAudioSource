using UnityEngine;
using UnityEngine.UI;
using System.Runtime.InteropServices;

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

		// BPM解析はまだ実験中。
		GetComponent<Text>().text = GetBPM().ToString();

		// BPM Scoreの最大値とその添字を取得
		var max = 0.0f;
		var max_index = 0;
		var min = Mathf.Infinity;
		for (var i = 0; i < 180 - 45; ++i) {
			if (GetBPMScore(i) > max) {
				max = GetBPMScore(i);
				max_index = i;
			}
			if (GetBPMScore(i) < min) {
				min = GetBPMScore(i);
			}
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
				if (i == 86 - 45) {
					color = Color.cyan;
				}
				Debug.DrawLine(
					new Vector3(i / 5.0f, (GetBPMScore(i) - min) / (max - min), 0),
					new Vector3(i / 5.0f, 0, 0),
					color);
			}
		}
		// Draw Estimated BPM
		Debug.DrawLine(
			new Vector3(max_index / 5.0f, (GetBPMScore(max_index) - min) / (max - min), 0),
			new Vector3(max_index / 5.0f, 0, 0),
			Color.yellow);

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
	}
}
