using UnityEngine;
using System.Collections;

public class EightBandLevelViewer : MonoBehaviour {
	public GameObject sound;
	public int band = 0;
	public Color color = Color.white;

	private LoopbackAudioSource source;

	// Use this for initialization
	void Start () {
		source = sound.GetComponent<LoopbackAudioSource>();
		GetComponent<MeshRenderer>().material.color = color;
	}
	
	// Update is called once per frame
	void Update () {
		transform.localScale = (new Vector3(1.0f, source.eightBandLevels[band] * 100.0f, 1.0f));
	}
}
