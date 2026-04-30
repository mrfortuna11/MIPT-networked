using System.Collections.Generic;
using UnityEngine;

namespace ProjectileNet.ClientSide
{
    public class ClientProjectile : MonoBehaviour
    {
        public enum Kind { PredictedFake, ServerDriven }

        public int     Id;
        public Kind    Mode;
        public Vector3 Velocity;
        public Vector3 Gravity = new Vector3(0f, -9.81f, 0f);

        public bool    HasServerSnapshot;
        public Vector3 LastServerPos;
        public Vector3 LastServerVel;
        public float   LastServerTime;

        public readonly List<Vector3> ClientTrail = new List<Vector3>(256);
        public readonly List<Vector3> ServerTrail = new List<Vector3>(256);

        public Color VisualColor = Color.white;

        private MeshRenderer mesh_renderer;

        private void Awake()
        {
            mesh_renderer = GetComponent<MeshRenderer>();
            if (mesh_renderer != null)
            {
                mesh_renderer.material.color = VisualColor;
            }

            ClientTrail.Add(transform.position);
        }

        public void SetColor(Color c)
        {
            VisualColor = c;
            if (mesh_renderer != null) mesh_renderer.material.color = c;
        }

        private void Update()
        {
            if (Mode == Kind.PredictedFake)
            {
                StepPredicted(Time.deltaTime);
            }
            else
            {
                StepServerDriven(Time.deltaTime);
            }
        }

        private void StepPredicted(float dt)
        {
            Velocity += Gravity * dt;
            transform.position += Velocity * dt;
            ClientTrail.Add(transform.position);

            if (HasServerSnapshot)
            {
                float since         = Time.time - LastServerTime;
                Vector3 expected    = LastServerPos + LastServerVel * since
                                       + 0.5f * Gravity * since * since;

                transform.position = Vector3.Lerp(transform.position, expected, 0.15f);
            }
        }

        private void StepServerDriven(float dt)
        {
            if (!HasServerSnapshot) return;

            float since      = Time.time - LastServerTime;
            Vector3 expected = LastServerPos + LastServerVel * since
                                + 0.5f * Gravity * since * since;

            transform.position = Vector3.Lerp(transform.position, expected, 0.5f);
            ClientTrail.Add(transform.position);
        }

        public void ApplySnapshot(Vector3 pos, Vector3 vel, float serverTime)
        {
            HasServerSnapshot = true;
            LastServerPos     = pos;
            LastServerVel     = vel;
            LastServerTime    = serverTime;
            ServerTrail.Add(pos);
        }
    }
}
