using System.Collections.Generic;
using UnityEngine;
using ProjectileNet.Net;

namespace ProjectileNet.ServerSide
{
    public class ServerWorld : MonoBehaviour
    {
        [Header("Simulation")]
        public Vector3 gravity      = new Vector3(0f, -9.81f, 0f);
        public float   defaultLife  = 5f;
        public float   projectileSpeed = 8f;

        [Header("Tick rates")]
        public int simHz      = 60;
        public int snapshotHz = 20;

        [Header("Wiring")]
        public NetworkSimulator network;

        private readonly Dictionary<int, ServerProjectile> projectiles = new Dictionary<int, ServerProjectile>();
        private float simAccumulator;
        private float snapshotAccumulator;
        private float serverTime;

        public IReadOnlyDictionary<int, ServerProjectile> Projectiles => projectiles;
        public float ServerTime => serverTime;

        private void Start()
        {
            if (network != null)
                network.OnCommandDelivered += HandleFireCommand;
            else
                Debug.LogError("[ServerWorld] network is null");
        }

        private void OnDestroy()
        {
            if (network != null)
                network.OnCommandDelivered -= HandleFireCommand;
        }

        private void Update()
        {
            network.DeliverIncomingCommands();

            float fixedStep = 1f / simHz;
            simAccumulator += Time.deltaTime;
            while (simAccumulator >= fixedStep)
            {
                StepProjectiles(fixedStep);
                simAccumulator -= fixedStep;
                serverTime     += fixedStep;
            }

            float snapStep = 1f / snapshotHz;
            snapshotAccumulator += Time.deltaTime;
            while (snapshotAccumulator >= snapStep)
            {
                BroadcastSnapshots();
                snapshotAccumulator -= snapStep;
            }
        }

        private void HandleFireCommand(FireCommand cmd)
        {
            if (projectiles.ContainsKey(cmd.projectileId)) return;

            Vector3 initialVelocity;
            if (cmd.mode == FireMode.ServerAuthoritative)
            {
                initialVelocity = cmd.direction.normalized * projectileSpeed;
            }
            else
            {
                initialVelocity = cmd.velocity;
            }

            var proj = new ServerProjectile
            {
                Id              = cmd.projectileId,
                Position        = cmd.origin,
                Velocity        = initialVelocity,
                SpawnServerTime = serverTime,
                LifetimeSec     = defaultLife,
            };
            proj.Trail.Add(proj.Position);

            if (cmd.mode == FireMode.ClientPredicted)
            {
                float lag = serverTime - cmd.clientFireTime;
                if (lag > 0f)
                    proj.FastForward(lag, gravity, 1f / simHz);
            }

            projectiles[proj.Id] = proj;
        }

        private void StepProjectiles(float dt)
        {
            List<int> toRemove = null;
            foreach (var kv in projectiles)
            {
                var p = kv.Value;
                p.Step(dt, gravity);
                if (!p.Alive)
                {
                    toRemove ??= new List<int>();
                    toRemove.Add(p.Id);
                }
            }

            if (toRemove != null)
            {
                foreach (var id in toRemove)
                {
                    var p = projectiles[id];
                    network.ServerSend(new ProjectileSnapshot
                    {
                        projectileId = p.Id,
                        position     = p.Position,
                        velocity     = p.Velocity,
                        serverTime   = serverTime,
                        destroyed    = true
                    });
                    projectiles.Remove(id);
                }
            }
        }

        private void BroadcastSnapshots()
        {
            foreach (var kv in projectiles)
            {
                var p = kv.Value;
                network.ServerSend(new ProjectileSnapshot
                {
                    projectileId = p.Id,
                    position     = p.Position,
                    velocity     = p.Velocity,
                    serverTime   = serverTime,
                    destroyed    = false
                });
            }
        }
    }
}
