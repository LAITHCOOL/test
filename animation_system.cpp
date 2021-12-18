
#define M_PI 3.14159265358979323846
constexpr unsigned int FNVHashEx(const char* const data, const unsigned int value = 2166136261)
{
    return (data[0] == '\0') ? value : (FNVHashEx(&data[1], (value * 16777619) ^ data[0]));
}
inline float NormalizeYaw(float yaw)
{
    if (yaw > 180)
        yaw -= (round(yaw / 360) * 360.f);
    else if (yaw < -180)
        yaw += (round(yaw / 360) * -360.f);

    return yaw;
}
bool playerStoppedMoving(player_t* pEntity)
{
    for (int w = 0; w < 13; w++)
    {
        AnimationLayer currentLayer = pEntity->get_animlayers()[1];
        const int activity = pEntity->sequence_activity(currentLayer.m_nSequence);
        float flcycle = currentLayer.m_flCycle, flprevcycle = currentLayer.m_flPrevCycle, flweight = currentLayer.m_flWeight, flweightdatarate = currentLayer.m_flWeightDeltaRate;
        uint32_t norder = currentLayer.m_nOrder;
        if (activity == ACT_CSGO_IDLE_ADJUST_STOPPEDMOVING)
            return true;
    }
    return false;
}
float GetCurtime()
{
    if (!g_csgo.m_engine()->IsConnected())
        return 0.f;

    if (!g_csgo.m_engine()->IsInGame())
        return 0.f;

    if (!g_ctx.m_local)
        return 0.f;

    return g_ctx.m_local->m_nTickBase() * g_csgo.m_globals()->m_interval_per_tick;
}

float GetLBYRotatedYaw(float lby, float yaw)
{
    float delta = NormalizeYaw(yaw - lby);
    if (fabs(delta) < 25.f)
        return lby;

    if (delta > 0.f)
        return yaw + 25.f;

    return yaw;
}
bool lowerBodyPrediction(player_t* pEntity)
{
    static float prediction = 0.f;
    static bool secondrun = false;
    float flServerTime = (float)pEntity->m_nTickBase() * g_csgo.m_globals()->m_interval_per_tick;
    if (playerStoppedMoving(pEntity) && !secondrun)
    {
        prediction = flServerTime + 0.22;
        secondrun = true;
    }
    else if (pEntity->m_vecVelocity().Length2D() < 0.1f && secondrun && prediction <= pEntity->m_flSimulationTime())
    {
        prediction = pEntity->m_nTickBase() + 1.1f;
    }
    else//theyre moving
    {
        secondrun = false;
        return false;
    }
    if (prediction <= pEntity->m_flSimulationTime())
    {
        return true;
    }
    return false;
}
inline float NormalizePitch(float pitch)
{
    while (pitch > 89.f)
        pitch -= 180.f;
    while (pitch < -89.f)
        pitch += 180.f;

    return pitch;
}

float __fastcall ang_dif(float a1, float a2)
{
    float val = fmodf(a1 - a2, 360.0);

    while (val < -180.0f) val += 360.0f;
    while (val > 180.0f) val -= 360.0f;

    return val;
}
bool adjusting_stop(player_t* player, AnimationLayer *layer)
{
    for (int i = 0; i < 15; i++)
    {
        for (int s = 0; s < 14; s++)
        {
            auto anim_layer = player->get_animlayer(s);
            if (!anim_layer.m_pOwner)
                continue;
            const int activity = player->sequence_activity(layer.m_nSequence);
            if (activity == 981 && anim_layer.m_flWeight == 1.f)
            {
                return true;
            }
        }
    }
    return false;
} // ACT_CSGO_FIRE_PRIMARY
float get_average_lby_standing_update_delta(player_t* player) {
    static float last_update_time[64];
    static float second_laste_update_time[64];
    static float oldlowerbody[64];
    float lby = static_cast<int>(fabs(player->get_eye_pos().y - player->m_flLowerBodyYawTarget()));

    if (lby != oldlowerbody[player->EntIndex()]) {
        second_laste_update_time[player->EntIndex()] = last_update_time[player->EntIndex()];
        last_update_time[player->EntIndex()] = g_csgo.m_globals()->m_curtime;
        oldlowerbody[player->EntIndex()] = lby;
    }

    return last_update_time[player->EntIndex()] - second_laste_update_time[player->EntIndex()];
}
float GetCurTime(CUserCmd* ucmd) {
    player_t* local_player = g_ctx.m_local;
    static int g_tick = 0;
    static CUserCmd* g_pLastCmd = nullptr;
    if (!g_pLastCmd || g_pLastCmd->m_predicted) {
        g_tick = (float)local_player->m_nTickBase();
    }
    else {
        // Required because prediction only runs on frames, not ticks
        // So if your framerate goes below tickrate, m_nTickBase won't update every tick
        ++g_tick;
    }
    g_pLastCmd = ucmd;
    float curtime = g_tick * g_csgo.m_globals()->m_interval_per_tick;
    return curtime;
}
Vector CalcAngle69(Vector dst, Vector src)
{
    Vector angles;

    double delta[3] = { (src.x - dst.x), (src.y - dst.y), (src.z - dst.z) };
    double hyp = sqrt(delta[0] * delta[0] + delta[1] * delta[1]);
    angles.x = (float)(atan(delta[2] / hyp) * 180.0 / 3.14159265);
    angles.y = (float)(atanf(delta[1] / delta[0]) * 57.295779513082f);
    angles.z = 0.0f;

    if (delta[0] >= 0.0)
    {
        angles.y += 180.0f;
    }

    return angles;
}
template<class T, class U>
inline T clamp(T in, U low, U high)
{
    if (in <= low)
        return low;
    else if (in >= high)
        return high;
    else
        return in;
}

float lerp_time()
{
    int ud_rate = g_csgo.m_cvar()->FindVar("cl_updaterate")->GetFloat();
    ConVar *min_ud_rate = g_csgo.m_cvar()->FindVar("sv_minupdaterate");
    ConVar *max_ud_rate = g_csgo.m_cvar()->FindVar("sv_maxupdaterate");
    if (min_ud_rate && max_ud_rate)
        ud_rate = max_ud_rate->GetFloat();
    float ratio = g_csgo.m_cvar()->FindVar("cl_interp_ratio")->GetFloat();
    if (ratio == 0)
        ratio = 1.0f;
    float lerp = g_csgo.m_cvar()->FindVar("cl_interp")->GetFloat();
    ConVar *c_min_ratio = g_csgo.m_cvar()->FindVar("sv_client_min_interp_ratio");
    ConVar *c_max_ratio = g_csgo.m_cvar()->FindVar("sv_client_max_interp_ratio");
    if (c_min_ratio && c_max_ratio && c_min_ratio->GetFloat() != 1)
        ratio = clamp(ratio, c_min_ratio->GetFloat(), c_max_ratio->GetFloat());
    return max(lerp, (ratio / ud_rate));
}
bool HasFakeHead(player_t* pEntity) {
    //lby should update if distance from lby to eye angles exceeds 35 degrees
    return abs(pEntity->m_angEyeAngles().y - pEntity->m_flLowerBodyYawTarget()) > 35;
}
bool Lbywithin35(player_t* pEntity) {
    //lby should update if distance from lby to eye angles less than 35 degrees
    return abs(pEntity->m_angEyeAngles().y - pEntity->m_flLowerBodyYawTarget()) < 35;
}
bool IsMovingOnGround(player_t* pEntity) {
    //Check if player has a velocity greater than 0 (moving) and if they are onground.
    return pEntity->m_vecVelocity().Length2D() > 45.f && pEntity->m_fFlags() & FL_ONGROUND;
}
bool IsMovingOnInAir(player_t* pEntity) {
    //Check if player has a velocity greater than 0 (moving) and if they are onground.
    return !(pEntity->m_fFlags() & FL_ONGROUND);
}
bool OnGround(player_t* pEntity) {
    //Check if player has a velocity greater than 0 (moving) and if they are onground.
    return pEntity->m_fFlags() & FL_ONGROUND;
}
bool IsFakeWalking(player_t* pEntity) {
    //Check if a player is moving, but at below a velocity of 36
    return IsMovingOnGround(pEntity) && pEntity->m_vecVelocity().Length2D() < 36.0f;
}

float tolerance = 10.f;
const inline float GetDelta(float a, float b) {
    return abs(NormalizeYaw(a - b));
}

const inline float LBYDelta(player_t* v) {
    return v->m_angEyeAngles().y - v->m_flLowerBodyYawTarget();
}

const inline bool IsDifferent(float a, float b, float tolerance = 10.f) {
    return (GetDelta(a, b) > tolerance);
}
bool HasStaticYawDifference(const std::deque<player_t*>& l, float tolerance) {
    for (auto i = l.begin(); i < l.end() - 1;)
    {
        if (GetDelta(LBYDelta(*i), LBYDelta(*++i)) > tolerance)
            return false;
    }
    return true;
}

int GetDifferentDeltas(const std::deque<player_t*>& l, float tolerance) {
    std::vector<float> vec;
    for (auto var : l) {
        float curdelta = LBYDelta(var);
        bool add = true;
        for (auto fl : vec) {
            if (!IsDifferent(curdelta, fl, tolerance))
                add = false;
        }
        if (add)
            vec.push_back(curdelta);
    }
    return vec.size();
}

int GetDifferentLBYs(const std::deque<player_t*>& l, float tolerance) {
    std::vector<float> vec;
    for (auto var : l)
    {
        float curyaw = var->m_flLowerBodyYawTarget();
        bool add = true;
        for (auto fl : vec)
        {
            if (!IsDifferent(curyaw, fl, tolerance))
                add = false;
        }
        if (add)
            vec.push_back(curyaw);
    }
    return vec.size();
}

bool DeltaKeepsChanging(const std::deque<player_t*>& cur, float tolerance) {
    return (GetDifferentDeltas(cur, tolerance) > (int)cur.size() / 2);
}

bool LBYKeepsChanging(const std::deque<player_t*>& cur, float tolerance) {
    return (GetDifferentLBYs(cur, tolerance) > (int)cur.size() / 2);
}
void LowerBodyYawFix(Vector* & Angle, player_t* Player)
{
    if (Player->m_vecVelocity().Length() > 1 && (Player->m_fFlags() & FL_ONGROUND))
        Angle->y = Player->m_flLowerBodyYawTarget();
}
static inline bool IsNearEqual(float v1, float v2, float Tolerance)
{
    return std::abs(v1 - v2) <= std::abs(Tolerance);
}
static int GetSequenceActivity(player_t* pEntity, int sequence)
{
    const model_t* pModel = pEntity->GetModel();
    if (!pModel)
        return 0;

    auto hdr = g_csgo.m_modelinfo()->GetStudiomodel(pEntity->GetModel());

    if (!hdr)
        return -1;

    static auto get_sequence_activity = reinterpret_cast<int(__fastcall*)(void*, studiohdr_t*, int)>(util::pattern_scan("client_panorama.dll", "55 8B EC 83 7D 08 FF 56 8B F1 74 3D"));

    return get_sequence_activity(pEntity, hdr, sequence);
}

int player_t::GetSequenceActivity(int sequence)
{
    auto hdr = g_csgo.m_modelinfo()->GetStudiomodel(this->GetModel());

    if (!hdr)
        return -1;

    static auto getSequenceActivity = (DWORD)(util::pattern_scan("client_panorama.dll", "55 8B EC 83 7D 08 FF 56 8B F1 74"));
    static auto GetSequenceActivity = reinterpret_cast<int(__fastcall*)(void*, studiohdr_t*, int)>(getSequenceActivity);

    return GetSequenceActivity(this, hdr, sequence);
}
bool lby_keeps_updating() {
    return get_average_lby_standing_update_delta;
}
bool IsAdjustingBalance(player_t* player, AnimationLayer *layer)
{
    for (int i = 0; i < 15; i++)
    {
        const int activity = player->sequence_activity(layer.m_nSequence);
        if (activity == 979)
        {
            return true;
        }
    }
    return false;
}
bool adjusting_balance(player_t * e, AnimationLayer * set) {
    const auto activity = e->sequence_activity(set[3].m_nSequence);

    if (activity == 979) {
        return true;
    }

    return false;
}

float NormalizeFloatToAngle(float input)
{
    for (auto i = 0; i < 3; i++) {
        while (input < -180.0f) input += 360.0f;
        while (input > 180.0f) input -= 360.0f;
    }
    return input;
}
float FixAusnahmeAngles(float yaw, float desyncdelta)
{
    for (auto i = 0; i < 3; i++) {
        while (yaw < -desyncdelta) yaw += desyncdelta;
        while (yaw > desyncdelta) yaw -= desyncdelta;
    }
    return yaw;
}
#define M_PI 3.14159265358979323846
void LBYBreakerCorrections(player_t* pEntity)
{
    float movinglby[64];
    float lbytomovinglbydelta[64];
    bool onground = pEntity->m_fFlags() & FL_ONGROUND;
    if (g_cfg.ragebot.correctlbybreaker)
    {
        lbytomovinglbydelta[pEntity->EntIndex()] = pEntity->m_flLowerBodyYawTarget() - lbytomovinglbydelta[pEntity->EntIndex()];

        if (pEntity->m_vecVelocity().Length2D() > 6 && pEntity->m_vecVelocity().Length2D() < 42)
        {
            pEntity->m_angEyeAngles().y = pEntity->m_flLowerBodyYawTarget() + 120;
        }
        else if (pEntity->m_vecVelocity().Length2D() < 6 || pEntity->m_vecVelocity().Length2D() > 42) // they are moving
        {
            pEntity->m_angEyeAngles().y = pEntity->m_flLowerBodyYawTarget();
            movinglby[pEntity->EntIndex()] = pEntity->m_flLowerBodyYawTarget();
        }
        else if (lbytomovinglbydelta[pEntity->EntIndex()] > 50 && lbytomovinglbydelta[pEntity->EntIndex()] < -50 &&
            lbytomovinglbydelta[pEntity->EntIndex()] < 112 && lbytomovinglbydelta[pEntity->EntIndex()] < -112) // the 50 will allow you to have a 30 degree margin of error (do the math :))
        {
            pEntity->m_angEyeAngles().y = movinglby[pEntity->EntIndex()];
        }
        else pEntity->m_angEyeAngles().y = pEntity->m_flLowerBodyYawTarget();
    }
}
void VectorAnglesBruteGay(const Vector& forward, Vector &angles)
{
    float tmp, yaw, pitch;
    if (forward[1] == 0 && forward[0] == 0)
    {
        yaw = 0;
        if (forward[2] > 0) pitch = 270; else pitch = 90;
    }
    else
    {
        yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
        if (yaw < 0) yaw += 360; tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]); pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
        if (pitch < 0) pitch += 360;
    } angles[0] = pitch; angles[1] = yaw; angles[2] = 0;
}

void AngleVectors(const Vector &angles, Vector *forward)
{
    Assert(s_bMathlibInitialized);
    Assert(forward);

    float    sp, sy, cp, cy;

    sy = sin(DEG2RAD(angles[1]));
    cy = cos(DEG2RAD(angles[1]));

    sp = sin(DEG2RAD(angles[0]));
    cp = cos(DEG2RAD(angles[0]));

    forward->x = cp * cy;
    forward->y = cp * sy;
    forward->z = -sp;
}

Vector calc_angle_trash(Vector src, Vector dst)
{
    Vector ret;
    VectorAnglesBruteGay(dst - src, ret);
    return ret;
}
void NormalizeNumX(Vector &vIn, Vector &vOut)
{
    float flLen = vIn.Length();
    if (flLen == 0) {
        vOut.Init(0, 0, 1);
        return;
    }
    flLen = 1 / flLen;
    vOut.Init(vIn.x * flLen, vIn.y * flLen, vIn.z * flLen);
}
float fov_entX(Vector ViewOffSet, Vector View, player_t* entity, int hitbox)
{
    const float MaxDegrees = 180.0f;
    Vector Angles = View, Origin = ViewOffSet;
    Vector Delta(0, 0, 0), Forward(0, 0, 0);
    Vector AimPos = entity->hitbox_position(hitbox);

    AngleVectors(Angles, &Forward);
    VectorSubtract(AimPos, Origin, Delta);
    NormalizeNumX(Delta, Delta);
    float DotProduct = Forward.Dot(Delta);
    return (acos(DotProduct) * (MaxDegrees / PI));
}

int closestX()
{
    int index = -1;
    float lowest_fov = 180.f; // maybe??

    player_t* local_player = g_ctx.m_local;

    if (!local_player)
        return -1;

    if (!local_player->is_alive())
        return -1;

    Vector local_position = local_player->GetAbsOrigin() + local_player->m_vecViewOffset();
    Vector angles;
    g_csgo.m_engine()->GetViewAngles(angles);
    for (int i = 1; i <= g_csgo.m_globals()->m_maxclients; i++)
    {
        auto entity = static_cast<player_t *>(g_csgo.m_entitylist()->GetClientEntity(i));

        if (!entity || entity->m_iHealth() <= 0 || entity->m_iTeamNum() == local_player->m_iTeamNum() || entity->IsDormant() || entity == local_player)
            continue;

        float fov = fov_entX(local_position, angles, entity, 0);
        if (fov < lowest_fov)
        {
            lowest_fov = fov;
            index = i;
        }
    }
    return index;
}
#define MASK_SHOT_BRUSHONLY            (CONTENTS_SOLID|CONTENTS_MOVEABLE|CONTENTS_WINDOW|CONTENTS_DEBRIS)

float apply_freestanding(player_t *enemy)
{
    auto local_player = g_ctx.m_local;
    if (!(local_player->m_iHealth() > 0))
        return 0.0f;
    bool no_active = true;
    float bestrotation = 0.f;
    float highestthickness = 0.f;
    static float hold = 0.f;
    Vector besthead;

    auto leyepos = enemy->m_vecOrigin() + enemy->m_vecViewOffset();
    auto headpos = enemy->hitbox_position(0);
    auto origin = enemy->m_vecOrigin();

    int index = closestX();

    if (index == -1)
        return 0.0f;

    if (enemy->EntIndex() == closestX())
    {
        auto checkWallThickness = [&](player_t* pPlayer, Vector newhead) -> float
        {
            Vector endpos1, endpos2;

            Vector eyepos = local_player->m_vecOrigin() + local_player->m_vecViewOffset();
            Ray_t ray;
            CTraceFilterSkipTwoEntities filter(local_player, enemy);
            trace_t trace1, trace2;

            ray.Init(newhead, eyepos);
            g_csgo.m_trace()->TraceRay(ray, MASK_SHOT_BRUSHONLY, &filter, &trace1);

            if (trace1.DidHit())
            {
                endpos1 = trace1.endpos;
                float add = newhead.DistTo(eyepos) - leyepos.DistTo(eyepos) + 75.f;
                return endpos1.DistTo(eyepos) + add / 2; // endpos2
            }

            else
            {
                endpos1 = trace1.endpos;
                float add = newhead.DistTo(eyepos) - leyepos.DistTo(eyepos) - 75.f;
                return endpos1.DistTo(eyepos) + add / 2; // endpos2
            }
        };

        float radius = Vector(headpos - origin).Length2D();

        for (float besthead = 0; besthead < 7; besthead += 0.1)
        {
            Vector newhead(radius * cos(besthead) + leyepos.x, radius * sin(besthead) + leyepos.y, leyepos.z);
            float totalthickness = 0.f;
            no_active = false;
            totalthickness += checkWallThickness(enemy, newhead);
            if (totalthickness > highestthickness)
            {
                highestthickness = totalthickness;

                bestrotation = besthead;
            }
        }
        return RAD2DEG(bestrotation);
    }
}
bool predict_lby(player_t* player, float oldlby[64], float lby, float speed)
{
    static bool nextflick[64];

    static float add_time[64];

    const auto sim = player->m_flSimulationTime();

    if (!g_cfg.ragebot.predictlbyupdate)
        return false;

    for (auto i = 0; i < g_csgo.m_globals()->m_maxclients; ++i)
    {
        if (oldlby != lby && speed <= 0.1f)
        {
            add_time = g_csgo.m_globals()->m_interval_per_tick + 1.1f;
        }

        if (speed >= 0.1f)
        {
            add_time = 0.22f;
            nextflick = sim + add_time;
        }

        if (sim >= nextflick && speed <= 0.1f)
        {
            add_time = 1.1f;
            nextflick = sim + add_time;
            return true;
        }
    }
    return false;
}

namespace UTILS
{
    float GetCurtime()
    {
        if (!g_csgo.m_engine()->IsConnected() || !g_csgo.m_engine()->IsInGame())
            return 0.f;

        if (!g_ctx.m_local)
            return 0.f;

        return g_ctx.m_local->m_nTickBase() * g_csgo.m_globals()->m_interval_per_tick;
    }
    float GetLBYRotatedYaw(float lby, float yaw)
    {
        float delta = MATH::NormalizeYaw(yaw - lby);
        if (fabs(delta) < 25.f)
            return lby;

        if (delta > 0.f)
            return yaw + 25.f;

        return yaw;
    }
}

namespace MATH
{
    float flAngleMod(float flAngle)
    {
        return((360.0f / 65536.0f) * ((int32_t)(flAngle * (65536.0f / 360.0f)) & 65535));
    }
    float ApproachAngle(float target, float value, float speed)
    {
        target = flAngleMod(target);
        value = flAngleMod(value);

        float delta = target - value;

        // Speed is assumed to be positive
        if (speed < 0)
            speed = -speed;

        if (delta < -180)
            delta += 360;
        else if (delta > 180)
            delta -= 360;

        if (delta > speed)
            value += speed;
        else if (delta < -speed)
            value -= speed;
        else
            value = target;

        return value;
    }

    void VectorAngles(const Vector& forward, Vector& angles)
    {
        float tmp, yaw, pitch;

        if (forward[1] == 0 && forward[0] == 0)
        {
            yaw = 0;
            if (forward[2] > 0)
                pitch = 270;
            else
                pitch = 90;
        }
        else
        {
            yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
            if (yaw < 0)
                yaw += 360;

            tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
            pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
            if (pitch < 0)
                pitch += 360;
        }

        angles[0] = pitch;
        angles[1] = yaw;
        angles[2] = 0;
    }

    void inline SinCos(float radians, float* sine, float* cosine)
    {
        *sine = sin(radians);
        *cosine = cos(radians);
    }
    float GRD_TO_BOG(float GRD) {
        return (PI / 180) * GRD;
    }
    void AngleVectors(const Vector& angles, Vector* forward, Vector* right, Vector* up)
    {
        float sr, sp, sy, cr, cp, cy;
        SinCos(DEG2RAD(angles[1]), &sy, &cy);
        SinCos(DEG2RAD(angles[0]), &sp, &cp);
        SinCos(DEG2RAD(angles[2]), &sr, &cr);

        if (forward)
        {
            forward->x = cp * cy;
            forward->y = cp * sy;
            forward->z = -sp;
        }
        if (right)
        {
            right->x = (-1 * sr * sp * cy + -1 * cr * -sy);
            right->y = (-1 * sr * sp * sy + -1 * cr * cy);
            right->z = -1 * sr * cp;
        }
        if (up)
        {
            up->x = (cr * sp * cy + -sr * -sy);
            up->y = (cr * sp * sy + -sr * cy);
            up->z = cr * cp;
        }
    }

    __forceinline float DotProduct(const float* a, const float* b)
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    __forceinline float DotProduct(const Vector& a, const Vector& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    void VectorTransform(const float* in1, const matrix3x4_t& in2, float* out)
    {
        out[0] = DotProduct(in1, in2[0]) + in2[0][3];
        out[1] = DotProduct(in1, in2[1]) + in2[1][3];
        out[2] = DotProduct(in1, in2[2]) + in2[2][3];
    }

    void VectorTransform(const Vector& in1, const matrix3x4_t& in2, Vector& out)
    {
        VectorTransform(&in1.x, in2, &out.x);
    }

    void VectorTransforma(const Vector& in1, const matrix3x4_t& in2, Vector& out)
    {
        VectorTransform(&in1.x, in2, &out.x);
    }

    float CalcAngle2D(const Vector2D& src, const Vector2D& dst)
    {
        float angle;
        VectorAngle2D(dst - src, angle);
        return angle;
   }
