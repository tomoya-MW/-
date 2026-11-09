// ====================================================
// 通信対戦（ホストがすべて計算し、クライアントは入力送信と表示のみ）
// ====================================================
#pragma once

static void SetNonBlocking(SOCKET s) { u_long on = 1; ioctlsocket(s, FIONBIO, &on); }
static void CloseSocket(SOCKET& s) { if (s != INVALID_SOCKET) { closesocket(s); s = INVALID_SOCKET; } }

static void StopNetwork(const char* message) {
    CloseSocket(g_peer);
    CloseSocket(g_listen);
    g_tx_used = 0;
    g_rx_used = 0;
    g_mode = MODE_NET_ERROR;
    sprintf_s(g_net_status, "%s (Escでメニューへ戻る)", message);
}

static void FlushSend() {
    if (g_peer == INVALID_SOCKET || g_tx_used == 0) return;
    int n = send(g_peer, g_tx, g_tx_used, 0);
    if (n == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) StopNetwork("送信エラー");
        return;
    }
    memmove(g_tx, g_tx + n, g_tx_used - n);
    g_tx_used -= n;
}

static void SendPacket(unsigned char type, const void* data, int len) {
    if (g_peer == INVALID_SOCKET) return;
    NetHeader h = { MAGIC, type, (unsigned short)len };
    if (g_tx_used + (int)sizeof(h) + len > (int)sizeof(g_tx)) {
        StopNetwork("送信が詰まりました（回線が遅すぎます）");
        return;
    }
    memcpy(g_tx + g_tx_used, &h, sizeof(h));
    g_tx_used += sizeof(h);
    if (len > 0) {
        memcpy(g_tx + g_tx_used, data, len);
        g_tx_used += len;
    }
    FlushSend();
}

static void SetupNetRound() {
    g_currentStageIdx = g_netRules.stage;
    g_gimmickEnabled = (g_netRules.gimmickOn != 0);
    InitStageGimmick(g_netRules.stage);
    ClearBattleObjects();
    int charaA = g_is_host ? g_myNetChara : g_enemyNetChara;
    int charaB = g_is_host ? g_enemyNetChara : g_myNetChara;
    Fighter& fa = g_is_host ? g_me : g_other;
    Fighter& fb = g_is_host ? g_other : g_me;
    ResetFighter(fa, 200.0f, 1, charaA, PlayableMaxHp(charaA));
    ResetFighter(fb, 440.0f, -1, charaB, PlayableMaxHp(charaB));
    ResetBattleGlobals();
    g_roundTime = (g_netRules.timeLimit == 0) ? 99 : g_netRules.timeLimit;
    g_netPhase = NP_READY;
    g_netPhaseTimer = 0;
    g_remoteQueueCount = 0;
    g_remoteBits = 0;
    UpdateCamera(true);
}

static void SetupNetMatch() {
    g_netRounds[0] = g_netRounds[1] = 0;
    g_netRoundNo = 1;
    g_netWinner = 0;
    g_netGotSnapshot = false;
    g_netFrame = 0;
    ResetMatchStats();
    SetupNetRound();
}

static void PackFighter(NetFighter& n, const Fighter& f) {
    n.x = f.x; n.y = f.y; n.vx = f.vx; n.vy = f.vy;
    n.hp = (short)ClampI(f.hp, -30000, 30000);
    n.maxHp = (short)f.maxHp;
    n.state = (unsigned char)f.state;
    n.move = (unsigned char)ClampI(f.move, 0, MS_COUNT - 1);
    n.variant = (unsigned char)f.variant;
    n.dir = (signed char)f.dir;
    n.chara = (unsigned char)f.charaIndex;
    n.flags = (unsigned char)((f.isGrounded ? 1 : 0) | (f.invulFrames > 0 ? 2 : 0) | (f.crouching ? 4 : 0) |
        (f.boarSuper ? 8 : 0) | (f.hasHit ? 16 : 0) | (f.whiffChecked ? 32 : 0) | (f.wallBounceUsed ? 64 : 0) | (f.connected ? 128 : 0));
    n.animTimer = (short)ClampI(f.animTimer, 0, 30000);
    n.stateTimer = (short)ClampI(f.stateTimer, 0, 30000);
    n.sp = (unsigned char)ClampI(f.spGauge, 0, 255);
    n.diceFace = (unsigned char)f.diceFace;
    n.diceShowTimer = (unsigned char)ClampI(f.diceShowTimer, 0, 255);
    n.stanceLevel = (unsigned char)f.stanceLevel;
    n.comboCount = (unsigned char)ClampI(f.comboCount, 0, 255);
    n.comboDisplayTimer = (unsigned char)ClampI(f.comboDisplayTimer, 0, 255);
    n.moveDir = (signed char)f.moveDir;
    n.labelSlot = (signed char)ClampI(f.labelSlot, -3, MS_COUNT - 1);
    n.boarTiming = (unsigned char)f.boarTiming;
    n.guardStun = (unsigned char)ClampI(f.guardStun, 0, 255);
    n.labelTimer = (unsigned char)ClampI(f.labelTimer, 0, 255);
}

static void UnpackFighter(Fighter& f, const NetFighter& n) {
    f.x = n.x; f.y = n.y; f.vx = n.vx; f.vy = n.vy;
    f.hp = n.hp; f.maxHp = (n.maxHp > 0) ? n.maxHp : 1;
    f.state = (ActionState)ClampI(n.state, 0, ACT_STATE_COUNT - 1);
    f.move = ClampI(n.move, 0, MS_COUNT - 1);
    f.variant = n.variant;
    f.dir = n.dir;
    f.charaIndex = ClampI(n.chara, 0, CHARA_COUNT - 1);
    f.isGrounded = (n.flags & 1) != 0;
    f.invulFrames = (n.flags & 2) ? 2 : 0;
    f.crouching = (n.flags & 4) != 0;
    f.boarSuper = (n.flags & 8) != 0;
    f.hasHit = (n.flags & 16) != 0;
    f.whiffChecked = (n.flags & 32) != 0;
    f.wallBounceUsed = (n.flags & 64) != 0;
    f.connected = (n.flags & 128) != 0;
    f.animTimer = n.animTimer;
    f.stateTimer = n.stateTimer;
    f.spGauge = n.sp;
    f.diceFace = n.diceFace;
    f.diceShowTimer = n.diceShowTimer;
    f.stanceLevel = n.stanceLevel;
    f.comboCount = n.comboCount;
    f.comboDisplayTimer = n.comboDisplayTimer;
    f.moveDir = n.moveDir;
    f.labelSlot = n.labelSlot;
    f.boarTiming = n.boarTiming;
    f.guardStun = n.guardStun;
    f.labelTimer = n.labelTimer;
}

static void PackStats(NetStats& n, const MatchStats& s) {
    n.moves = (short)s.moves; n.hits = (short)s.hits; n.taken = (short)s.taken;
    n.guards = (short)s.guards; n.parries = (short)s.parries; n.maxCombo = (short)s.maxCombo;
    n.damage = s.damage;
}

static void UnpackStats(MatchStats& s, const NetStats& n) {
    s.moves = n.moves; s.hits = n.hits; s.taken = n.taken;
    s.guards = n.guards; s.parries = n.parries; s.maxCombo = n.maxCombo;
    s.damage = n.damage;
}

static void SendSnapshot() {
    static NetSnapshot s;
    memset(&s, 0, sizeof(s));
    s.frame = ++g_netFrame;
    PackFighter(s.a, g_me);
    PackFighter(s.b, g_other);
    for (int i = 0; i < MAX_SHOTS; i++) {
        if (!g_shots[i].active) continue;
        NetShot& ns = s.shots[s.shotCount++];
        ns.x = g_shots[i].x; ns.y = g_shots[i].y; ns.color = g_shots[i].color;
        ns.type = (unsigned char)g_shots[i].projType; ns.sub = (unsigned char)g_shots[i].subType;
        ns.radius = (unsigned char)g_shots[i].radius; ns.fromA = g_shots[i].isFromMe ? 1 : 0;
    }
    for (int i = 0; i < MAX_TRAPS; i++) {
        if (!g_traps[i].active) continue;
        NetTrap& nt = s.traps[s.trapCount++];
        nt.x = g_traps[i].x; nt.type = (unsigned char)g_traps[i].type; nt.ownerA = g_traps[i].ownerIsMe ? 1 : 0;
        nt.life = (short)g_traps[i].life; nt.armDelay = (short)g_traps[i].armDelay;
    }
    s.gx = g_gimmick.x; s.gFallAngle = g_gimmick.fallAngle; s.itemX = g_gimmick.itemX;
    s.ghp = (short)g_gimmick.hp; s.gmaxHp = (short)g_gimmick.maxHp;
    s.gFlags = (unsigned char)((g_gimmick.isFalling ? 1 : 0) | (g_gimmick.fallen ? 2 : 0) | (g_gimmick.itemActive ? 4 : 0) |
        (g_gimmick.hazardActive ? 8 : 0) | (g_gimmick.hazardWarning ? 16 : 0));
    s.gDir = (signed char)g_gimmick.activeHitBox;
    s.gStageTimer = g_gimmick.stageTimer;
    s.roundTime = (signed char)g_roundTime;
    s.readyTimer = (unsigned char)ClampI(g_readyFightTimer, 0, 255);
    s.phase = (unsigned char)g_netPhase;
    s.phaseTimer = (unsigned char)ClampI(g_netPhaseTimer, 0, 255);
    s.roundsA = (unsigned char)g_netRounds[0];
    s.roundsB = (unsigned char)g_netRounds[1];
    s.roundNo = (unsigned char)g_netRoundNo;
    s.winner = (unsigned char)g_netWinner;
    s.cutinTimer = (unsigned char)ClampI(g_superCutinTimer, 0, 255);
    s.cutinFlags = (unsigned char)((g_superIsUltimate ? 1 : 0) | (g_cutinIsCounter ? 2 : 0));
    strncpy_s(s.cutinText, g_superCutinName, _TRUNCATE);
    s.shake = (unsigned char)ClampI(g_screenShakeTimer, 0, 255);
    s.flash = (unsigned char)ClampI(g_flashTimer, 0, 255);
    s.strobe = (unsigned char)ClampI(g_kokusenStrobe, 0, 255);
    s.parryNotice = (unsigned char)ClampI(g_parryNoticeTimer, 0, 255);
    s.kokusen = g_isKokusenKoFinish ? 1 : 0;
    s.counterTimerA = (unsigned char)ClampI(g_counterLabelTimer[0], 0, 255);
    s.counterTypeA = (unsigned char)g_counterLabelType[0];
    s.counterTimerB = (unsigned char)ClampI(g_counterLabelTimer[1], 0, 255);
    s.counterTypeB = (unsigned char)g_counterLabelType[1];
    PackStats(s.statsA, g_stats[0]);
    PackStats(s.statsB, g_stats[1]);
    s.fxCount = (unsigned char)g_netFxCount;
    memcpy(s.fx, g_netFxQueue, sizeof(NetFxEvent) * g_netFxCount);
    s.popupCount = (unsigned char)g_netPopupCount;
    memcpy(s.popups, g_netPopupQueue, sizeof(NetPopupEvent) * g_netPopupCount);
    g_netFxCount = 0;
    g_netPopupCount = 0;
    SendPacket(PKT_SNAPSHOT, &s, sizeof(s));
}

// クライアント：ホストから届いた状態をそのまま表示に反映（自分 = B）
static void ApplySnapshot(const NetSnapshot& s) {
    UnpackFighter(g_me, s.b);
    UnpackFighter(g_other, s.a);
    for (int i = 0; i < MAX_SHOTS; i++) g_shots[i].active = false;
    for (int i = 0; i < s.shotCount && i < MAX_SHOTS; i++) {
        const NetShot& ns = s.shots[i];
        Projectile& p = g_shots[i];
        p.active = true; p.x = ns.x; p.y = ns.y; p.color = ns.color;
        p.projType = ns.type; p.subType = ns.sub; p.radius = ns.radius;
        p.isFromMe = (ns.fromA == 0);
        p.vx = p.vy = p.gravity = 0; p.damage = 0; p.life = 2;
    }
    for (int i = 0; i < MAX_TRAPS; i++) g_traps[i].active = false;
    for (int i = 0; i < s.trapCount && i < MAX_TRAPS; i++) {
        const NetTrap& nt = s.traps[i];
        Trap& t = g_traps[i];
        t.active = true; t.x = nt.x; t.type = nt.type; t.ownerIsMe = (nt.ownerA == 0);
        t.life = nt.life; t.armDelay = nt.armDelay;
    }
    g_gimmick.x = s.gx; g_gimmick.fallAngle = s.gFallAngle; g_gimmick.itemX = s.itemX; g_gimmick.itemY = GROUND_Y;
    g_gimmick.hp = s.ghp; g_gimmick.maxHp = (s.gmaxHp > 0) ? s.gmaxHp : 1;
    g_gimmick.isFalling = (s.gFlags & 1) != 0;
    g_gimmick.fallen = (s.gFlags & 2) != 0;
    g_gimmick.itemActive = (s.gFlags & 4) != 0;
    g_gimmick.hazardActive = (s.gFlags & 8) != 0;
    g_gimmick.hazardWarning = (s.gFlags & 16) != 0;
    g_gimmick.activeHitBox = s.gDir;
    g_gimmick.stageTimer = s.gStageTimer;
    if (g_gimmick.hazardWarning) {
        int phase = g_gimmick.stageTimer % HAZARD_PERIOD;
        g_gimmick.hazardCountdown = (HAZARD_ACTIVE_START - phase + 59) / 60;
    }
    g_roundTime = s.roundTime;
    g_readyFightTimer = s.readyTimer;
    g_netPhase = (NetPhase)s.phase;
    g_netPhaseTimer = s.phaseTimer;
    g_netRounds[0] = s.roundsA;
    g_netRounds[1] = s.roundsB;
    g_netRoundNo = s.roundNo;
    g_netWinner = s.winner;
    g_superCutinTimer = s.cutinTimer;
    g_superIsUltimate = (s.cutinFlags & 1) != 0;
    g_cutinIsCounter = (s.cutinFlags & 2) != 0;
    strncpy_s(g_netCutinText, s.cutinText, _TRUNCATE);
    g_superCutinName = g_netCutinText;
    g_screenShakeTimer = s.shake;
    g_flashTimer = s.flash;
    g_kokusenStrobe = s.strobe;
    g_parryNoticeTimer = s.parryNotice;
    g_isKokusenKoFinish = (s.kokusen != 0);
    g_counterLabelTimer[0] = s.counterTimerB; g_counterLabelType[0] = s.counterTypeB;
    g_counterLabelTimer[1] = s.counterTimerA; g_counterLabelType[1] = s.counterTypeA;
    UnpackStats(g_stats[0], s.statsB);
    UnpackStats(g_stats[1], s.statsA);
    for (int i = 0; i < s.fxCount && i < NET_MAX_FX; i++) {
        const NetFxEvent& e = s.fx[i];
        if (e.kind == 0) SpawnHitFX(e.x, e.y, e.color, e.count);
        else SpawnSparks(e.x, e.y, e.color, e.count, (float)e.len);
    }
    for (int i = 0; i < s.popupCount && i < NET_MAX_POPUPS; i++) {
        char text[49];
        memcpy(text, s.popups[i].text, 48);
        text[48] = '\0';
        SpawnPopup(s.popups[i].x, s.popups[i].y, text, s.popups[i].color);
    }
    g_netGotSnapshot = true;
}

static void ProcessPacket(unsigned char type, const char* data, int len) {
    switch (type) {
    case PKT_SETUP:
        if (len == (int)sizeof(NetSetup) && !g_is_host) {
            memcpy(&g_netRules, data, sizeof(NetSetup));
            g_netRules.stage = (unsigned char)ClampI(g_netRules.stage, 0, 4);
            g_netRules.roundsToWin = (unsigned char)ClampI(g_netRules.roundsToWin, 1, 3);
            g_myNetReady = false;
            g_enemyNetReady = false;
            g_mode = MODE_NET_CHARA_SELECT;
        }
        break;
    case PKT_CHARA:
        if (len == (int)sizeof(NetCharaMsg)) {
            g_enemyNetChara = ClampI(((const NetCharaMsg*)data)->chara, 0, CHARA_COUNT - 1);
            g_enemyNetReady = true;
        }
        break;
    case PKT_INPUT:
        if (len == (int)sizeof(NetInputMsg) && g_is_host) {
            if (g_remoteQueueCount >= 16) {
                memmove(g_remoteQueue, g_remoteQueue + 1, sizeof(unsigned short) * 15);
                g_remoteQueueCount = 15;
            }
            g_remoteQueue[g_remoteQueueCount++] = ((const NetInputMsg*)data)->bits;
        }
        break;
    case PKT_SNAPSHOT:
        if (len == (int)sizeof(NetSnapshot) && !g_is_host && g_mode == MODE_NET_BATTLE_PLAY) {
            ApplySnapshot(*(const NetSnapshot*)data);
        }
        break;
    default:
        break;
    }
}

static void ReceivePackets() {
    if (g_peer == INVALID_SOCKET) return;
    for (;;) {
        int space = (int)sizeof(g_rx) - g_rx_used;
        if (space <= 0) { StopNetwork("受信バッファがあふれました"); return; }
        int n = recv(g_peer, g_rx + g_rx_used, space, 0);
        if (n == 0) { StopNetwork("相手が切断しました"); return; }
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEWOULDBLOCK) break;
            StopNetwork("受信エラー");
            return;
        }
        g_rx_used += n;
    }
    int pos = 0;
    while (g_rx_used - pos >= (int)sizeof(NetHeader)) {
        NetHeader h;
        memcpy(&h, g_rx + pos, sizeof(h));
        if (h.magic != MAGIC) { StopNetwork("不正なデータを受信しました"); return; }
        if (g_rx_used - pos < (int)sizeof(NetHeader) + h.len) break;
        ProcessPacket(h.type, g_rx + pos + sizeof(NetHeader), h.len);
        if (g_peer == INVALID_SOCKET) return;
        pos += sizeof(NetHeader) + h.len;
    }
    if (pos > 0) {
        memmove(g_rx, g_rx + pos, g_rx_used - pos);
        g_rx_used -= pos;
    }
}

static void HandleTextInput(char* buf, int maxLen, bool allowDot) {
    if (s_keyCooldown > 0) { s_keyCooldown--; return; }
    int len = (int)strlen(buf);
    char pressed = 0;
    static const int keys[10] = { KEY_INPUT_0, KEY_INPUT_1, KEY_INPUT_2, KEY_INPUT_3, KEY_INPUT_4, KEY_INPUT_5, KEY_INPUT_6, KEY_INPUT_7, KEY_INPUT_8, KEY_INPUT_9 };
    static const int pads[10] = { KEY_INPUT_NUMPAD0, KEY_INPUT_NUMPAD1, KEY_INPUT_NUMPAD2, KEY_INPUT_NUMPAD3, KEY_INPUT_NUMPAD4, KEY_INPUT_NUMPAD5, KEY_INPUT_NUMPAD6, KEY_INPUT_NUMPAD7, KEY_INPUT_NUMPAD8, KEY_INPUT_NUMPAD9 };
    for (int i = 0; i < 10; i++) {
        if (CheckHitKey(keys[i]) || CheckHitKey(pads[i])) { pressed = (char)('0' + i); break; }
    }
    if (!pressed && allowDot && (CheckHitKey(KEY_INPUT_PERIOD) || CheckHitKey(KEY_INPUT_DECIMAL))) pressed = '.';
    if (pressed != 0 && len < maxLen - 1) {
        buf[len] = pressed; buf[len + 1] = '\0';
        s_keyCooldown = 10;
    }
    if (CheckHitKey(KEY_INPUT_BACK) && len > 0) { buf[len - 1] = '\0'; s_keyCooldown = 10; }
}

static void OnPeerConnected() {
    int one = 1;
    setsockopt(g_peer, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
    SetNonBlocking(g_peer);
    g_rx_used = 0;
    g_tx_used = 0;
    g_myNetReady = false;
    g_enemyNetReady = false;
    g_netRuleRow = 0;
    g_mode = MODE_NET_STAGE_SELECT;
    SendPacket(PKT_HELLO, NULL, 0);
}

static void StartHost() {
    g_is_host = true;
    g_is_net_battle = true;
    g_net_room_code = 1000 + rand() % 9000;
    g_listen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen == INVALID_SOCKET) { StopNetwork("ソケット初期化失敗"); return; }
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_addr.s_addr = INADDR_ANY; a.sin_port = htons(PORT);
    if (bind(g_listen, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR || listen(g_listen, 1) == SOCKET_ERROR) {
        StopNetwork("ポート27015開放失敗");
        return;
    }
    SetNonBlocking(g_listen);
    g_mode = MODE_HOST_WAIT;
}

static void UpdateHostWait() {
    SOCKET s = accept(g_listen, NULL, NULL);
    if (s != INVALID_SOCKET) {
        g_peer = s;
        CloseSocket(g_listen);
        OnPeerConnected();
    }
}

static void StartClient(const char* ip) {
    g_is_host = false;
    g_is_net_battle = true;
    g_peer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_peer == INVALID_SOCKET) { StopNetwork("ソケット作成失敗"); return; }
    sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) { StopNetwork("IPアドレス書式不正"); return; }
    if (connect(g_peer, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR) { StopNetwork("接続失敗"); return; }
    OnPeerConnected();
}

// ホスト：両者の入力で1フレーム進め、結果を送る
static void NetHostUpdate() {
    unsigned short localBits = (g_cmdListOpen || g_paused) ? 0 : ReadLocalBits();
    if (g_remoteQueueCount > 0) {
        if (g_remoteQueueCount > 4) {
            int drop = g_remoteQueueCount - 2;
            memmove(g_remoteQueue, g_remoteQueue + drop, sizeof(unsigned short) * (g_remoteQueueCount - drop));
            g_remoteQueueCount -= drop;
        }
        g_remoteBits = g_remoteQueue[0];
        memmove(g_remoteQueue, g_remoteQueue + 1, sizeof(unsigned short) * (g_remoteQueueCount - 1));
        g_remoteQueueCount--;
    }

    g_netRecordFx = true;
    switch (g_netPhase) {
    case NP_READY:
    case NP_FIGHT:
        if (g_netPhase == NP_READY) {
            g_readyFightTimer++;
            if (g_readyFightTimer == 50) TriggerScreenShake(8);
            if (g_readyFightTimer >= 80) g_netPhase = NP_FIGHT;
        }
        else if (g_netRules.timeLimit > 0) {
            if (++g_battleTimerFrames >= 60) {
                g_battleTimerFrames = 0;
                if (g_roundTime > 0) g_roundTime--;
            }
        }
        UpdateControlledFighter(g_me, g_other, g_ctrl[0], localBits);
        UpdateControlledFighter(g_other, g_me, g_ctrl[1], g_remoteBits);
        ResolvePushboxes();
        UpdateTraps();
        UpdateStageGimmick(g_currentStageIdx);
        UpdateShots();
        UpdateEffects();
        if (g_netPhase == NP_FIGHT) {
            bool ko = (g_me.hp <= 0 || g_other.hp <= 0);
            bool timeUp = (g_netRules.timeLimit > 0 && g_roundTime <= 0);
            if (ko || timeUp) {
                float ra = (float)g_me.hp / (float)g_me.maxHp;
                float rb = (float)g_other.hp / (float)g_other.maxHp;
                int w;
                if (ko) w = (g_me.hp <= 0 && g_other.hp <= 0) ? 3 : ((g_me.hp > 0) ? 1 : 2);
                else w = (fabsf(ra - rb) < 0.001f) ? 3 : ((ra > rb) ? 1 : 2);
                if (w == 1) g_netRounds[0]++;
                if (w == 2) g_netRounds[1]++;
                g_netWinner = w;
                g_netPhase = NP_ROUND_END;
                g_netPhaseTimer = 0;
                TriggerScreenShake(16);
            }
        }
        break;
    case NP_ROUND_END:
        UpdateEffects();
        if (++g_netPhaseTimer >= 120) {
            bool over = g_netRounds[0] >= g_netRules.roundsToWin || g_netRounds[1] >= g_netRules.roundsToWin;
            if (over) {
                g_netWinner = (g_netRounds[0] > g_netRounds[1]) ? 1 : ((g_netRounds[1] > g_netRounds[0]) ? 2 : 3);
                g_netPhase = NP_MATCH_END;
                g_netPhaseTimer = 0;
            }
            else {
                g_netRoundNo++;
                g_netWinner = 0;
                SetupNetRound();
            }
        }
        break;
    case NP_MATCH_END:
        UpdateEffects();
        g_netPhaseTimer++;
        break;
    }
    g_netRecordFx = false;
    SendSnapshot();
    if (g_netPhase == NP_MATCH_END && g_netPhaseTimer >= 90) {
        g_roundWon = (g_netWinner == 1);
        g_mode = MODE_NET_RESULT;
    }
}

// クライアント：入力を送り、届いた状態を表示する
static void NetClientUpdate() {
    unsigned short bits = (g_cmdListOpen || g_paused) ? 0 : ReadLocalBits();
    NetInputMsg msg = { ++g_sequence, bits };
    SendPacket(PKT_INPUT, &msg, sizeof(msg));
    ReceivePackets();
    UpdateEffects();
    if (g_netPhase == NP_MATCH_END && g_netPhaseTimer >= 80) {
        g_roundWon = (g_netWinner == 2);
        g_mode = MODE_NET_RESULT;
    }
}

static void DrawNetPhaseBanner() {
    int myWinner = g_is_host ? 1 : 2;
    if (g_netPhase == NP_ROUND_END) {
        DrawBox(170, 180, 470, 260, BLACK, TRUE);
        DrawBox(170, 180, 470, 260, RED, FALSE);
        const char* text = (g_netWinner == 3) ? "DRAW" : ((g_netWinner == myWinner) ? "ROUND WIN!" : "ROUND LOSE…");
        DrawString(270, 200, (g_me.hp <= 0 || g_other.hp <= 0) ? "K. O." : "TIME OVER", YELLOW);
        DrawString(260, 228, text, (g_netWinner == myWinner) ? YELLOW : GRAY);
    }
    else if (g_netPhase == NP_MATCH_END) {
        DrawBox(150, 175, 490, 265, BLACK, TRUE);
        DrawBox(150, 175, 490, 265, YELLOW, FALSE);
        const char* text = (g_netWinner == 3) ? "DRAW GAME" : ((g_netWinner == myWinner) ? "YOU WIN!!" : "YOU LOSE…");
        DrawExtendString(230, 205, 2.0, 2.0, text, (g_netWinner == myWinner) ? YELLOW : RED);
    }
}
