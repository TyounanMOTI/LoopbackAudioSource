using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;

public class PeakSphere : MonoBehaviour {
	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static void Initialize(int sampling_rate);

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static float GetOutsidePeakMeter();

	// Use this for initialization
	void Start () {
		Initialize(AudioSettings.outputSampleRate);
	}
	
	// Update is called once per frame
	void Update () {
		transform.localScale = Vector3.one * GetOutsidePeakMeter();
	}
}
