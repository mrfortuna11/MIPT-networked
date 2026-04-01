using UnityEngine;
using System;

public class ServerObject : MonoBehaviour
{
    [Header("Position")]
    public float yOffset = 0f;

    [Header("Movement")]
    public float speed = 2f;
    public float amplitude = 3f;

    [Header("Replication")]
    public float sendInterval = 0.1f; 

    private float _timer;

    public static event Action<Vector3, Vector3, float> OnSnapshotSent;

    void Update()
    {
        float x = Mathf.Sin(Time.time * speed) * amplitude;
        transform.position = new Vector3(x, yOffset, 0f);

        _timer += Time.deltaTime;
        if (_timer >= sendInterval)
        {
            _timer = 0f;
            SendSnapshot();
        }
    }

    void SendSnapshot()
    {
        Vector3 pos = transform.position;
        Vector3 vel = GetComponent<Rigidbody>() != null
            ? GetComponent<Rigidbody>().linearVelocity
            : new Vector3(Mathf.Cos(Time.time * speed) * speed * amplitude, 0f, 0f);

        OnSnapshotSent?.Invoke(pos, vel, Time.time);
    }
}