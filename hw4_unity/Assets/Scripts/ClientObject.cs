using UnityEngine;
using System.Collections.Generic;

public enum ClientMode { Interpolation, Extrapolation }

public class ClientObject : MonoBehaviour
{
    [Header("Position")]
    public float yOffset = 0f;

    [Header("Mode")]
    public ClientMode mode = ClientMode.Interpolation;

    [Header("Interpolation")]
    [Tooltip("Задержка буфера (сек)")]
    public float interpolationDelay = 0.15f;

    [Header("Visual")]
    public Renderer rend;

    private readonly List<Snapshot> _buffer = new List<Snapshot>();

    private Snapshot _latest;
    private Snapshot _prev;
    private bool _hasTwo;

    void OnEnable() => ServerObject.OnSnapshotSent += ReceiveSnapshot;
    void OnDisable() => ServerObject.OnSnapshotSent -= ReceiveSnapshot;

    void Start()
    {
        if (rend != null)
            rend.material.color = mode == ClientMode.Interpolation
                ? Color.green
                : Color.red;
    }

    void ReceiveSnapshot(Vector3 pos, Vector3 vel, float timestamp)
    {
        var snap = new Snapshot(timestamp, pos, vel);

        _buffer.Add(snap);

        _prev = _latest;
        _latest = snap;
        if (!_hasTwo && _buffer.Count >= 2) _hasTwo = true;
    }

    void Update()
    {
        if (mode == ClientMode.Interpolation)
            ApplyInterpolation();
        else
            ApplyExtrapolation();
    }

    void ApplyInterpolation()
    {
        if (_buffer.Count < 2) return;

        float renderTime = Time.time - interpolationDelay;

        Snapshot from = _buffer[0];
        Snapshot to = _buffer[1];

        for (int i = 0; i < _buffer.Count - 1; i++)
        {
            if (_buffer[i].timestamp <= renderTime && renderTime <= _buffer[i + 1].timestamp)
            {
                from = _buffer[i];
                to = _buffer[i + 1];
                break;
            }
        }

        while (_buffer.Count > 2 && _buffer[0].timestamp < renderTime - interpolationDelay)
            _buffer.RemoveAt(0);

        float duration = to.timestamp - from.timestamp;
        if (duration <= 0f) { transform.position = to.position; return; }

        float t = Mathf.Clamp01((renderTime - from.timestamp) / duration);
        Vector3 lerped = Vector3.Lerp(from.position, to.position, t);
        transform.position = new Vector3(lerped.x, yOffset, 0f);
    }

    void ApplyExtrapolation()
    {
        if (!_hasTwo) return;

        float dt = Time.time - _latest.timestamp;

        Vector3 predicted = _latest.position + _latest.velocity * dt;
        transform.position = new Vector3(predicted.x, yOffset, 0f);
    }
}