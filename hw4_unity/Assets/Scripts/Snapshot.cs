public struct Snapshot
{
    public float timestamp;
    public UnityEngine.Vector3 position;
    public UnityEngine.Vector3 velocity;

    public Snapshot(float t, UnityEngine.Vector3 pos, UnityEngine.Vector3 vel)
    {
        timestamp = t;
        position = pos;
        velocity = vel;
    }
}