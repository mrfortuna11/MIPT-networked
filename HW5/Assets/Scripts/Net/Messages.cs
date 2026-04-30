using UnityEngine;

namespace ProjectileNet.Net
{
    public enum FireMode
    {
        ServerAuthoritative,
        ClientPredicted    
    }

    public struct FireCommand
    {
        public int      projectileId;
        public Vector3  origin;
        public Vector3  direction;     
        public Vector3  velocity;      
        public float    clientFireTime;
        public FireMode mode;
    }

    public struct ProjectileSnapshot
    {
        public int     projectileId;
        public Vector3 position;
        public Vector3 velocity;
        public float   serverTime;
        public bool    destroyed;
    }
}
