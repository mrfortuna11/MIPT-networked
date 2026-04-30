using System;
using System.Collections.Generic;
using UnityEngine;

namespace ProjectileNet.Net
{
    public class NetworkSimulator : MonoBehaviour
    {
        [Tooltip("Односторонняя задержка в миллисекундах. RTT будет вдвое больше")]
        [Range(0f, 500f)]
        public float oneWayLatencyMs = 80f;

        public event Action<FireCommand>        OnCommandDelivered;
        public event Action<ProjectileSnapshot> OnSnapshotDelivered;

        private struct CmdEnvelope    { public float deliverAt; public FireCommand        msg; }
        private struct SnapEnvelope   { public float deliverAt; public ProjectileSnapshot msg; }

        private readonly Queue<CmdEnvelope>  _c2s = new Queue<CmdEnvelope>();
        private readonly Queue<SnapEnvelope> _s2c = new Queue<SnapEnvelope>();

        public void ClientSend(FireCommand cmd)
        {
            _c2s.Enqueue(new CmdEnvelope
            {
                deliverAt = Time.time + oneWayLatencyMs * 0.001f,
                msg       = cmd
            });
        }

        public void ServerSend(ProjectileSnapshot snap)
        {
            _s2c.Enqueue(new SnapEnvelope
            {
                deliverAt = Time.time + oneWayLatencyMs * 0.001f,
                msg       = snap
            });
        }

        public void DeliverIncomingCommands()
        {
            while (_c2s.Count > 0 && _c2s.Peek().deliverAt <= Time.time)
            {
                var env = _c2s.Dequeue();
                OnCommandDelivered?.Invoke(env.msg);
            }
        }

        public void DeliverIncomingSnapshots()
        {
            while (_s2c.Count > 0 && _s2c.Peek().deliverAt <= Time.time)
            {
                var env = _s2c.Dequeue();
                OnSnapshotDelivered?.Invoke(env.msg);
            }
        }
    }
}
