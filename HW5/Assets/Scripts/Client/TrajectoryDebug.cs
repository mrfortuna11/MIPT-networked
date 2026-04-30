using System.Collections.Generic;
using UnityEngine;
using ProjectileNet.ClientSide;
using ProjectileNet.ServerSide;

namespace ProjectileNet.Debugging
{
    public class TrajectoryDebug : MonoBehaviour
    {
        public ClientWorld clientWorld;
        public ServerWorld serverWorld;

        public Color clientTrailColor = new Color(0.2f, 1f, 0.2f);
        public Color serverTrailColor = new Color(1f, 0.3f, 0.3f);
        public Color snapshotColor    = new Color(1f, 1f, 0.2f);

        public int decimation = 1;

        private Material lineMat;

        private void Awake()
        {
            var shader = Shader.Find("Hidden/Internal-Colored");
            lineMat = new Material(shader) { hideFlags = HideFlags.HideAndDontSave };
            lineMat.SetInt("_SrcBlend", (int)UnityEngine.Rendering.BlendMode.SrcAlpha);
            lineMat.SetInt("_DstBlend", (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            lineMat.SetInt("_Cull",     (int)UnityEngine.Rendering.CullMode.Off);
            lineMat.SetInt("_ZWrite",   0);
            lineMat.SetInt("_ZTest",    (int)UnityEngine.Rendering.CompareFunction.Always);
        }

        private void Update()
        {
            if (clientWorld != null)
            {
                foreach (var kv in clientWorld.FakeProjectiles)
                    DrawTrailDebug(kv.Value.ClientTrail, clientTrailColor);

                foreach (var kv in clientWorld.ServerProjectiles)
                    DrawTrailDebug(kv.Value.ServerTrail, serverTrailColor);
            }

            if (serverWorld != null)
            {
                foreach (var kv in serverWorld.Projectiles)
                    DrawTrailDebug(kv.Value.Trail, serverTrailColor);
            }
        }

        private void OnRenderObject()
        {
            if (lineMat == null) return;
            lineMat.SetPass(0);
            GL.PushMatrix();
            GL.MultMatrix(Matrix4x4.identity);

            GL.Begin(GL.LINES);

            if (clientWorld != null)
            {
                foreach (var kv in clientWorld.FakeProjectiles)
                    DrawTrailGL(kv.Value.ClientTrail, clientTrailColor);

                foreach (var kv in clientWorld.ServerProjectiles)
                    DrawTrailGL(kv.Value.ServerTrail, serverTrailColor);
            }

            if (serverWorld != null)
            {
                foreach (var kv in serverWorld.Projectiles)
                    DrawTrailGL(kv.Value.Trail, serverTrailColor);
            }

            GL.End();

            GL.Begin(GL.LINES);
            if (clientWorld != null)
            {
                foreach (var kv in clientWorld.ServerProjectiles)
                    DrawSnapshotMarkersGL(kv.Value.ServerTrail, snapshotColor);
            }
            GL.End();

            GL.PopMatrix();
        }

        private void DrawTrailDebug(List<Vector3> pts, Color color)
        {
            for (int i = 1; i < pts.Count; i += decimation)
                Debug.DrawLine(pts[i - 1], pts[i], color);
        }

        private void DrawTrailGL(List<Vector3> pts, Color color)
        {
            GL.Color(color);
            for (int i = 1; i < pts.Count; i += decimation)
            { GL.Vertex(pts[i - 1]); GL.Vertex(pts[i]); }
        }

        private void DrawSnapshotMarkersGL(List<Vector3> pts, Color color)
        {
            GL.Color(color);
            const float s = 0.08f;
            foreach (var p in pts)
            {
                GL.Vertex(p + new Vector3(-s, 0, 0)); GL.Vertex(p + new Vector3(s, 0, 0));
                GL.Vertex(p + new Vector3(0, -s, 0)); GL.Vertex(p + new Vector3(0, s, 0));
                GL.Vertex(p + new Vector3(0, 0, -s)); GL.Vertex(p + new Vector3(0, 0, s));
            }
        }

        private void OnDestroy()
        {
            if (lineMat != null) Destroy(lineMat);
        }
    }
}
