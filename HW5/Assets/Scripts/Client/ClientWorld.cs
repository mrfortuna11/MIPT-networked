using System.Collections.Generic;
using UnityEngine;
using ProjectileNet.Net;

namespace ProjectileNet.ClientSide
{
    public class ClientWorld : MonoBehaviour
    {
        public NetworkSimulator network;

        [Header("Visual")]
        public float projectileScale   = 0.25f;
        public Color colorPredicted    = new Color(0.2f, 1f, 0.2f);
        public Color colorServerDriven = new Color(1f, 0.35f, 0.35f);

        private readonly Dictionary<int, ClientProjectile> fakePredicted = new();
        private readonly Dictionary<int, ClientProjectile> serverDriven  = new();

        public IReadOnlyDictionary<int, ClientProjectile> ServerProjectiles => serverDriven;
        public IReadOnlyDictionary<int, ClientProjectile> FakeProjectiles   => fakePredicted;

        private void Start()
        {
            if (network != null)
                network.OnSnapshotDelivered += HandleSnapshot;
            else
                Debug.LogError("[ClientWorld] network is null");
        }

        private void OnDestroy()
        {
            if (network != null)
                network.OnSnapshotDelivered -= HandleSnapshot;
        }

        private void Update()
        {
            network.DeliverIncomingSnapshots();
        }

        public ClientProjectile SpawnPredicted(int id, Vector3 origin, Vector3 velocity, Vector3 gravity)
        {
            var go = CreateVisual(origin, $"Fake_{id}", PrimitiveType.Cube,
                                  projectileScale * 1.4f, colorPredicted);
            var cp = go.AddComponent<ClientProjectile>();
            cp.Id       = id;
            cp.Mode     = ClientProjectile.Kind.PredictedFake;
            cp.Velocity = velocity;
            cp.Gravity  = gravity;
            cp.SetColor(colorPredicted);
            fakePredicted[id] = cp;
            return cp;
        }

        private ClientProjectile SpawnServerDriven(int id, Vector3 pos, Vector3 vel,
                                                   Vector3 gravity, float serverTime)
        {
            var go = CreateVisual(pos, $"Server_{id}", PrimitiveType.Sphere,
                                  projectileScale, colorServerDriven);
            var cp = go.AddComponent<ClientProjectile>();
            cp.Id       = id;
            cp.Mode     = ClientProjectile.Kind.ServerDriven;
            cp.Velocity = vel;
            cp.Gravity  = gravity;
            cp.ApplySnapshot(pos, vel, serverTime);
            cp.SetColor(colorServerDriven);
            serverDriven[id] = cp;
            return cp;
        }

        private GameObject CreateVisual(Vector3 pos, string name,
                                        PrimitiveType shape, float scale, Color color)
        {
            var go = GameObject.CreatePrimitive(shape);
            go.name = name;
            go.transform.position   = pos;
            go.transform.localScale = Vector3.one * scale;
            var col = go.GetComponent<Collider>();
            if (col != null) Destroy(col);
            var mr = go.GetComponent<MeshRenderer>();
            mr.material = new Material(mr.sharedMaterial) { color = color };
            return go;
        }

        private void HandleSnapshot(ProjectileSnapshot snap)
        {
            if (snap.destroyed)
            {
                if (serverDriven.TryGetValue(snap.projectileId, out var sd))
                {
                    sd.ApplySnapshot(snap.position, snap.velocity, snap.serverTime);
                    Destroy(sd.gameObject);
                    serverDriven.Remove(snap.projectileId);
                }
                if (fakePredicted.TryGetValue(snap.projectileId, out var fk))
                {
                    Destroy(fk.gameObject);
                    fakePredicted.Remove(snap.projectileId);
                }
                return;
            }

            if (serverDriven.TryGetValue(snap.projectileId, out var existing))
            {
                existing.ApplySnapshot(snap.position, snap.velocity, snap.serverTime);
            }
            else
            {
                SpawnServerDriven(snap.projectileId, snap.position, snap.velocity,
                                  new Vector3(0f, -9.81f, 0f), snap.serverTime);
            }
        }
    }
}
