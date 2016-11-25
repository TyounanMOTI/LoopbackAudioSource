using UnityEngine;
using System.Collections;

public class BeatSphere : MonoBehaviour {

	// Use this for initialization
	void Start () {
		BeatTracker.OnBeat += ColorChangeOnBeat;
	}
	
	// Update is called once per frame
	void ColorChangeOnBeat() {
		GetComponent<MeshRenderer>().material.color = Color.magenta;
		StartCoroutine("ResetColor");
	}

	IEnumerator ResetColor()
	{
		yield return new WaitForSeconds(0.1f);
		GetComponent<MeshRenderer>().material.color = Color.white;
	}
}
