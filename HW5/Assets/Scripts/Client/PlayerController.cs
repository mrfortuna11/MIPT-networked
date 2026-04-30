using UnityEngine;
using UnityEngine.InputSystem;
using ProjectileNet.Net;

namespace ProjectileNet.ClientSide
{
    public class PlayerController : MonoBehaviour
    {
        [Header("Wiring")]
        public ClientWorld      clientWorld;
        public NetworkSimulator network;
        public Camera           cam;
        public Transform        muzzle;

        [Header("Tuning")]
        public float   projectileSpeed = 8f;
        public float   moveSpeed       = 6f;
        public Vector3 gravity         = new Vector3(0f, -9.81f, 0f);

        [Header("Mouse Look")]
        public float mouseSensitivity = 0.15f;
        public float pitchClamp       = 80f;

        [Header("State")]
        public FireMode currentMode = FireMode.ClientPredicted;

        private static int _nextId = 1;
        public int ShotsFired { get; private set; }

        private Keyboard keyboard;
        private Mouse    mouse;
        private float    _pitch = 15f;

        private struct ShotRecord { public Vector3 origin; public Vector3 clientDir; public float firedAt; }
        private readonly System.Collections.Generic.Dictionary<int, ShotRecord> shots = new();

        private void Start()
        {
            keyboard    = Keyboard.current;
            mouse = Mouse.current;
            Cursor.lockState = CursorLockMode.Locked;
            Cursor.visible   = false;
        }

        private void Update()
        {
            if (keyboard    == null) keyboard    = Keyboard.current;
            if (mouse == null) mouse = Mouse.current;
            HandleCursorToggle();
            HandleMouseLook();
            HandleMovement();
            HandleModeSwitch();
            HandleFire();
            DrawPredictionRays();
        }

        private void HandleCursorToggle()
        {
            if (keyboard != null && keyboard.escapeKey.wasPressedThisFrame)
            { Cursor.lockState = CursorLockMode.None; Cursor.visible = true; }
            if (mouse != null && mouse.leftButton.wasPressedThisFrame
                && Cursor.lockState == CursorLockMode.None)
            { Cursor.lockState = CursorLockMode.Locked; Cursor.visible = false; }
        }

        private void HandleMouseLook()
        {
            if (mouse == null || Cursor.lockState != CursorLockMode.Locked) return;
            Vector2 delta = mouse.delta.ReadValue() * mouseSensitivity;
            transform.Rotate(Vector3.up, delta.x, Space.World);
            _pitch = Mathf.Clamp(_pitch - delta.y, -pitchClamp, pitchClamp);
            if (cam != null) cam.transform.localRotation = Quaternion.Euler(_pitch, 0f, 0f);
        }

        private void HandleMovement()
        {
            if (keyboard == null) return;
            float h = (keyboard.dKey.isPressed || keyboard.rightArrowKey.isPressed ? 1f : 0f)
                    - (keyboard.aKey.isPressed || keyboard.leftArrowKey.isPressed  ? 1f : 0f);
            float v = (keyboard.wKey.isPressed || keyboard.upArrowKey.isPressed   ? 1f : 0f)
                    - (keyboard.sKey.isPressed || keyboard.downArrowKey.isPressed  ? 1f : 0f);
            Vector3 move = (transform.right * h + transform.forward * v) * moveSpeed * Time.deltaTime;
            move.y = 0f;
            transform.position += move;
        }

        private void HandleModeSwitch()
        {
            if (keyboard == null || !keyboard.tabKey.wasPressedThisFrame) return;
            currentMode = currentMode == FireMode.ServerAuthoritative
                ? FireMode.ClientPredicted : FireMode.ServerAuthoritative;
            Debug.Log($"[Player] Mode -> {currentMode}");
        }

        private void HandleFire()
        {
            if (mouse == null) return;
            if (Cursor.lockState != CursorLockMode.Locked) return;
            if (!mouse.leftButton.wasPressedThisFrame) return;

            Vector3 dir    = cam != null ? cam.transform.forward : transform.forward;
            Vector3 origin = muzzle != null ? muzzle.position : transform.position;

            int id = _nextId++;
            ShotsFired++;
            shots[id] = new ShotRecord { origin = origin, clientDir = dir, firedAt = Time.time };

            network.ClientSend(new FireCommand
            {
                projectileId   = id,
                origin         = origin,
                direction      = dir,                    
                velocity       = dir * projectileSpeed,  
                                                        
                clientFireTime = Time.time,
                mode           = currentMode
            });

            if (currentMode == FireMode.ClientPredicted)
                clientWorld.SpawnPredicted(id, origin, dir * projectileSpeed, gravity);
        }

        private void DrawPredictionRays()
        {
            const float rayLen = 15f;
            const float maxAge = 4f;

            var green = new Color(0.2f, 1f, 0.2f);
            foreach (var kv in shots)
            {
                if (Time.time - kv.Value.firedAt > maxAge) continue;
                Debug.DrawRay(kv.Value.origin, kv.Value.clientDir * rayLen, green);
            }

            if (clientWorld == null) return;
            var red = new Color(1f, 0.3f, 0.3f);
            foreach (var kv in clientWorld.ServerProjectiles)
            {
                var cp = kv.Value;
                if (!cp.HasServerSnapshot) continue;
                Debug.DrawRay(cp.LastServerPos, cp.LastServerVel.normalized * rayLen, red);
            }
        }
    }
}
