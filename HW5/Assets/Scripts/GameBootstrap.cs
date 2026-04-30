using UnityEngine;
using ProjectileNet.Net;
using ProjectileNet.ClientSide;
using ProjectileNet.ServerSide;
using ProjectileNet.Debugging;

namespace ProjectileNet
{
    public class GameBootstrap : MonoBehaviour
    {
        [Header("Bootstrap settings")]
        public float oneWayLatencyMs = 80f;
        public int   serverSimHz     = 60;
        public int   serverSnapshotHz = 20;

        public Vector3 gravity = new Vector3(0f, -9.81f, 0f);

        public NetworkSimulator Net    { get; private set; }
        public ServerWorld      Server { get; private set; }
        public ClientWorld      Client { get; private set; }
        public PlayerController Player { get; private set; }

        private GUIStyle _hudStyle;

        private void Awake()
        {
            BuildSceneObjects();
            BuildNetworkLayer();
            BuildServer();
            BuildClient();
            BuildPlayer();
            BuildDebug();
        }

        private void BuildSceneObjects()
        {
            var floor = GameObject.CreatePrimitive(PrimitiveType.Plane);
            floor.name = "Floor";
            floor.transform.localScale = Vector3.one * 5f;
            floor.GetComponent<MeshRenderer>().material.color = new Color(0.18f, 0.2f, 0.22f);

            var lightGo = new GameObject("Sun");
            var light = lightGo.AddComponent<Light>();
            light.type = LightType.Directional;
            light.transform.rotation = Quaternion.Euler(50f, 30f, 0f);
            light.intensity = 1.1f;

            for (int i = 0; i < 5; i++)
            {
                var pillar = GameObject.CreatePrimitive(PrimitiveType.Cube);
                pillar.name = "Pillar_" + i;
                pillar.transform.position   = new Vector3(-6f + i * 3f, 1f, 12f);
                pillar.transform.localScale = new Vector3(0.6f, 2f, 0.6f);
                pillar.GetComponent<MeshRenderer>().material.color =
                    new Color(0.4f, 0.45f, 0.5f);
            }
        }

        private void BuildNetworkLayer()
        {
            var go = new GameObject("[Network]");
            Net = go.AddComponent<NetworkSimulator>();
            Net.oneWayLatencyMs = oneWayLatencyMs;
        }

        private void BuildServer()
        {
            var go = new GameObject("[Server]");
            Server = go.AddComponent<ServerWorld>();
            Server.network    = Net;
            Server.gravity    = gravity;
            Server.simHz      = serverSimHz;
            Server.snapshotHz = serverSnapshotHz;
        }

        private void BuildClient()
        {
            var go = new GameObject("[Client]");
            Client = go.AddComponent<ClientWorld>();
            Client.network = Net;
        }

        private void BuildPlayer()
        {
            var player = GameObject.CreatePrimitive(PrimitiveType.Capsule);
            player.name = "Player";
            player.transform.position = new Vector3(0f, 1f, 0f);
            Destroy(player.GetComponent<Collider>());
            player.GetComponent<MeshRenderer>().material.color = new Color(0.3f, 0.6f, 1f);

            var camGo = new GameObject("MainCamera");
            camGo.tag = "MainCamera";
            var cam = camGo.AddComponent<Camera>();
            cam.transform.SetParent(player.transform, false);
            cam.transform.localPosition = new Vector3(0f, 0.6f, 0f);
            cam.transform.localRotation = Quaternion.Euler(15f, 0f, 0f);
            cam.backgroundColor = new Color(0.08f, 0.1f, 0.13f);
            cam.clearFlags = CameraClearFlags.SolidColor;

            var muzzle = new GameObject("Muzzle").transform;
            muzzle.SetParent(cam.transform, false);
            muzzle.localPosition = new Vector3(0.2f, -0.15f, 0.5f);

            Player = player.AddComponent<PlayerController>();
            Player.clientWorld = Client;
            Player.network     = Net;
            Player.cam         = cam;
            Player.muzzle      = muzzle;
            Player.gravity     = gravity;
        }

        private void BuildDebug()
        {
            var cam = Camera.main;
            var dbg = cam.gameObject.AddComponent<TrajectoryDebug>();
            dbg.clientWorld = Client;
            dbg.serverWorld = Server;
        }


        private void OnGUI()
        {
            if (_hudStyle == null)
            {
                _hudStyle = new GUIStyle(GUI.skin.label)
                {
                    fontSize  = 14,
                    fontStyle = FontStyle.Bold,
                    normal    = { textColor = Color.white }
                };
            }

            var rect = new Rect(12, 12, 460, 220);
            GUI.Box(rect, GUIContent.none);

            GUILayout.BeginArea(new Rect(rect.x + 8, rect.y + 8, rect.width - 16, rect.height - 16));

            GUILayout.Label($"Mode: <color=#FFCC33>{Player.currentMode}</color> (Tab to switch)", new GUIStyle(_hudStyle) { richText = true });
            GUILayout.Label($"Shots fired: {Player.ShotsFired}",   _hudStyle);
            GUILayout.Label($"Server projectiles: {Server.Projectiles.Count}", _hudStyle);
            GUILayout.Label($"Client visuals:     {Client.ServerProjectiles.Count}", _hudStyle);
            GUILayout.Space(6);

            GUILayout.Label($"One-way latency: {Net.oneWayLatencyMs:F0} ms (RTT ≈ {Net.oneWayLatencyMs * 2f:F0} ms)", _hudStyle);
            Net.oneWayLatencyMs = GUILayout.HorizontalSlider(Net.oneWayLatencyMs, 0f, 500f);

            GUILayout.Space(6);
            GUILayout.Label("LMB: fire    WASD: move    Tab: toggle mode", _hudStyle);
            GUILayout.Label("<color=#33FF55>Green</color>=client predicted, <color=#FF5555>Red</color>=server authoritative, <color=#FFFF33>Yellow×</color>=snapshot",
                            new GUIStyle(_hudStyle) { richText = true });

            GUILayout.EndArea();

            float cx = Screen.width  * 0.5f;
            float cy = Screen.height * 0.5f;
            GUI.Label(new Rect(cx - 4, cy - 10, 20, 20), "+", _hudStyle);
        }
    }
}
