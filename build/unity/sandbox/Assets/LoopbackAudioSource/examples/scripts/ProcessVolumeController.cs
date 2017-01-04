using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using System.Runtime.InteropServices;

public class ProcessVolumeController : MonoBehaviour {
	public int processId;
	public float volume;

	[DllImport("AudioPlugin_LoopbackAudioSource")]
	public extern static void SetProcessVolume(int processId, float volume);

	// Update is called once per frame
	void Update () {
		SetProcessVolume(processId, volume);
	}
}
