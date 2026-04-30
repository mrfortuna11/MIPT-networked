using System.Collections.Generic;
using UnityEngine;

namespace ProjectileNet.ServerSide
{
    public class ServerProjectile
    {
        public int     Id;
        public Vector3 Position;
        public Vector3 Velocity;
        public float   SpawnServerTime;
        public float   LifetimeSec;
        public bool    Alive = true;

        public readonly List<Vector3> Trail = new List<Vector3>(256);

        public void Step(float dt, Vector3 gravity)
        {
            if (!Alive) return;

            Velocity += gravity * dt;
            Position += Velocity * dt;

            Trail.Add(Position);

            if (Position.y <= 0f)
            {
                Position = new Vector3(Position.x, 0f, Position.z);
                Alive    = false;
            }

            LifetimeSec -= dt;
            if (LifetimeSec <= 0f) Alive = false;
        }

        public void FastForward(float extraSec, Vector3 gravity, float fixedStep)
        {
            float remaining = extraSec;
            while (remaining > 0f && Alive)
            {
                float dt = Mathf.Min(fixedStep, remaining);
                Step(dt, gravity);
                remaining -= dt;
            }
        }
    }
}
