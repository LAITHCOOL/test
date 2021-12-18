#include  "../c_hooks.h"
#include "c_animations.h"
#include "../ragebot/c_backtrack.h"
#include "../ragebot/c_resolver.h"
#include "../ragebot/c_ragebot.h"
#include "../ragebot/c_antiAims.h"

CMAnimationFix* g_Animfix = new CMAnimationFix();
CResolver* resolver = new CResolver();

float calculate_lerp()
{
	static auto cl_interp = interfaces.cvars->FindVar("cl_interp"); //-V807
	static auto cl_interp_ratio = interfaces.cvars->FindVar("cl_interp_ratio");
	static auto sv_client_min_interp_ratio = interfaces.cvars->FindVar("sv_client_min_interp_ratio");
	static auto sv_client_max_interp_ratio = interfaces.cvars->FindVar("sv_client_max_interp_ratio");
	static auto cl_updaterate = interfaces.cvars->FindVar("cl_updaterate");
	static auto sv_minupdaterate = interfaces.cvars->FindVar("sv_minupdaterate");
	static auto sv_maxupdaterate = interfaces.cvars->FindVar("sv_maxupdaterate");

	auto updaterate = std::clamp(cl_updaterate->GetFloat(), sv_minupdaterate->GetFloat(), sv_maxupdaterate->GetFloat());
	auto lerp_ratio = std::clamp(cl_interp_ratio->GetFloat(), sv_client_min_interp_ratio->GetFloat(), sv_client_max_interp_ratio->GetFloat());

	return std::clamp(lerp_ratio / updaterate, cl_interp->GetFloat(), 1.0f);
}

void Extrapolate(IBasePlayer* player, Vector& origin, Vector& velocity, int& flags, bool on_ground)
{
	static const auto sv_gravity = interfaces.cvars->FindVar(hs::sv_gravity::s().c_str());
	static const auto sv_jump_impulse = interfaces.cvars->FindVar(hs::sv_jump_impulse::s().c_str());

	if (!(flags & FL_ONGROUND))
		velocity.z -= TICKS_TO_TIME(sv_gravity->GetFloat());
	else if (player->GetFlags() & FL_ONGROUND && !on_ground)
		velocity.z = sv_jump_impulse->GetFloat();

	const auto src = origin;
	auto end = src + velocity * interfaces.global_vars->interval_per_tick;

	Ray_t r;
	r.Init(src, end, player->GetMins(), player->GetMaxs());

	trace_t t;
	CTraceFilter filter;
	filter.pSkip = player;

	interfaces.trace->TraceRay(r, MASK_PLAYERSOLID, &filter, &t);

	if (t.fraction != 1.f)
	{
		for (auto i = 0; i < 2; i++)
		{
			velocity -= t.plane.normal * velocity.Dot(t.plane.normal);

			const auto dot = velocity.Dot(t.plane.normal);
			if (dot < 0.f)
				velocity -= Vector(dot * t.plane.normal.x,
					dot * t.plane.normal.y, dot * t.plane.normal.z);

			end = t.endpos + velocity * TICKS_TO_TIME(1.f - t.fraction);

			r.Init(t.endpos, end, player->GetMins(), player->GetMaxs());
			interfaces.trace->TraceRay(r, MASK_PLAYERSOLID, &filter, &t);

			if (t.fraction == 1.f)
				break;
		}
	}

	origin = end = t.endpos;
	end.z -= 2.f;

	r.Init(origin, end, player->GetMins(), player->GetMaxs());
	interfaces.trace->TraceRay(r, MASK_PLAYERSOLID, &filter, &t);

	flags &= ~FL_ONGROUND;

	if (t.DidHit() && t.plane.normal.z > .7f)
		flags |= FL_ONGROUND;
}

/*bool animation::is_valid(float range = .2f, float max_unlag = .2f)
{
	if (!interfaces.engine->GetNetChannelInfo() || !valid)
		return false;

	const auto correct = std::clamp(interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING)
		+ interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING)
		+ calculate_lerp(), 0.f, max_unlag);
	if (CanDT() && csgo->dt_charged && !CMAntiAim::Get().did_shot && !vars.ragebot.fast_recovery)
		range += TICKS_TO_TIME(8);
	return fabsf(correct - (interfaces.global_vars->curtime - sim_time)) < range && correct < 1.f;
}*/
bool animation::is_valid(float range = .2f, float max_unlag = .2f)
{
	if (!interfaces.engine->GetNetChannelInfo() || !valid)
		return false;

	auto correct = interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) + interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING) + calculate_lerp();

	static auto sv_maxunlag = interfaces.cvars->FindVar("sv_maxunlag");
	correct = std::clamp(correct, 0.0f, sv_maxunlag->GetFloat());

	if (CanDT() && csgo->dt_charged && !CMAntiAim::Get().did_shot && vars.ragebot.double_tap)
		range += TICKS_TO_TIME(8);

	return fabsf(correct - (interfaces.global_vars->curtime - sim_time)) < range && correct < 1.f;
}
animation::animation(IBasePlayer* player)
{
	const auto weapon = player->GetWeapon();

	this->player = player;
	index = player->GetIndex();
	dormant = player->IsDormant();
	velocity = player->GetVelocity();
	origin = player->GetOrigin();
	abs_origin = player->GetAbsOrigin();
	obb_mins = player->GetMins();
	obb_maxs = player->GetMaxs();
	memcpy(layers, player->GetAnimOverlays(), sizeof(CAnimationLayer) * 13);
	poses = player->m_flPoseParameter();
	anim_state = player->GetPlayerAnimState();
	sim_time = player->GetSimulationTime();
	interp_time = 0.f;
	last_shot_time = weapon ? weapon->GetLastShotTime() : 0.f;
	duck = player->GetDuckAmount();
	lby = player->GetLBY();
	eye_angles = player->GetEyeAngles();
	abs_ang = player->GetAbsAngles();
	flags = player->GetFlags();
	eflags = player->GetEFlags();
	effects = player->GetEffects();

	lag = TIME_TO_TICKS(player->GetSimulationTime() - player->GetOldSimulationTime());

	// animations are off when we enter pvs, we do not want to shoot yet.
	valid = lag >= 0 && lag <= 17;

	// clamp it so we don't interpolate too far : )
	lag = std::clamp(lag, 0, 17);
}

animation::animation(IBasePlayer* player, Vector last_reliable_angle) : animation(player)
{
	this->last_reliable_angle = last_reliable_angle;
}

void animation::restore(IBasePlayer* player) const
{
	player->GetFlagsPtr() = flags;
	player->GetDuckAmount() = duck;
	memcpy(player->GetAnimOverlays(), layers, sizeof(CAnimationLayer) * 13);
	player->GetOrigin() = origin;
	player->SetAbsOrigin(abs_origin);
}

void animation::apply(IBasePlayer* player) const
{
	*player->GetEyeAnglesPointer() = eye_angles;
	player->GetDuckAmount() = duck;
	player->GetFlagsPtr() = flags;
	player->GetOrigin() = origin;
	player->SetAbsOrigin(abs_origin);
}

void animation::build_server_bones(IBasePlayer* player)
{
	const auto backup_occlusion_flags = player->GetOcclusionFlags();
	const auto backup_occlusion_framecount = player->GetOcclusionFramecount();

	player->GetOcclusionFlags() = 0;
	player->GetOcclusionFramecount() = 0;

	player->GetReadableBones() = player->GetWritableBones() = 0;

	player->InvalidateBoneCache();

	player->GetEffects() |= 0x8;

	const auto backup_bone_array = player->GetBoneArrayForWrite();
	player->GetBoneArrayForWrite() = bones;

	csgo->UpdateMatrix = true;
	player->SetupBones(nullptr, -1, 0x7FF00, player->GetSimulationTime());
	csgo->UpdateMatrix = false;

	player->GetBoneArrayForWrite() = backup_bone_array;
	player->GetOcclusionFlags() = backup_occlusion_flags;
	player->GetOcclusionFramecount() = backup_occlusion_framecount;

	player->GetEffects() &= ~0x8;
}

void CMAnimationFix::animation_info::update_animations(animation* record, animation* from)
{
	auto Interpolate = [](const Vector from, const Vector to, const float percent) {
		return to * percent + from * (1.f - percent);
	};
	auto Interpolate2 = [](const float from, const float to, const float percent) {
		return to * percent + from * (1.f - percent);
	};

	if (!from)
	{
		// set velocity and layers.
		record->velocity = player->GetVelocity();

		// fix feet spin.
		record->anim_state->m_flFeetYawRate = 0.f;

		// apply record.
		record->apply(player);

		// run update.
		return g_Animfix->update_player(player, record, from);
	}

	const auto new_velocity = player->GetVelocity();

	// restore old record.

	memcpy(player->GetAnimOverlays(), from->layers, sizeof(CAnimationLayer) * 13);
	player->SetAbsOrigin(record->origin);
	player->SetAbsAngles(from->abs_ang);
	player->GetVelocity() = from->velocity;

	// setup velocity.
	record->velocity = new_velocity;

	// did the player shoot?

	// setup extrapolation parameters.
	auto old_origin = from->origin;
	auto old_flags = from->flags;



	for (auto i = 0; i < record->lag; i++)
	{

		// move time forward.
		const auto time = from->sim_time + TICKS_TO_TIME(i + 1);
		const auto lerp = 1.f - (record->sim_time - time) / (record->sim_time - from->sim_time);

		player->GetDuckAmount() = Interpolate2(from->duck, record->duck, lerp);

		// resolve player.
		if (record->lag - 1 == i)
		{
			player->GetVelocity() = new_velocity;
			player->GetFlagsPtr() = record->flags;
		}
		else // compute velocity and flags.
		{
			Extrapolate(player, old_origin, player->GetVelocity(), player->GetFlagsPtr(), old_flags & FL_ONGROUND);
			old_flags = player->GetFlags();
		}

		record->resolver = ResolverMode[player->GetIndex()];

		player->GetPlayerAnimState()->m_flFeetYawRate = 0.f;

		// backup simtime.
		const auto backup_simtime = player->GetSimulationTime();

		// set new simtime.
		player->GetSimulationTime() = time;

		// run update.
		g_Animfix->update_player(player, record, from);

		// restore old simtime.
		player->GetSimulationTime() = backup_simtime;
	}
	if (!record->dormant && !from->dormant)
		record->didshot = record->last_shot_time > from->sim_time && record->last_shot_time <= record->sim_time;
}

void CMAnimationFix::UpdatePlayers()
{
	if (!interfaces.engine->IsInGame())
		return;

	if (!csgo->local) {
		if (!animation_infos.empty()) {
			animation_infos.clear();
		}

		return;
	}

	//// erase outdated entries
	for (auto it = animation_infos.begin(); it != animation_infos.end();) {
		auto player = reinterpret_cast<IBasePlayer*>(interfaces.ent_list->GetClientEntityFromHandle(it->first));

		if (!player || player != it->second.player || !player->isAlive()
			|| !csgo->local)
		{
			if (player)
				player->GetClientSideAnims() = true;
			it = animation_infos.erase(it);
		}
		else
			it = next(it);
	}

	for (auto i = 1; i <= interfaces.engine->GetMaxClients(); ++i) {
		const auto entity = interfaces.ent_list->GetClientEntity(i);
		if (!entity || !entity->IsPlayer())
			continue;
		if (!entity->isAlive() || entity->IsDormant())//++hh
			continue;

		if (entity == csgo->local)
			continue;

		if (entity != csgo->local && entity->GetTeam() == csgo->local->GetTeam()) {
			csgo->EnableBones = entity->GetClientSideAnims() = true;
			continue;
		}

		if (animation_infos.find(entity->GetRefEHandle()) == animation_infos.end())
			animation_infos.insert_or_assign(entity->GetRefEHandle(), animation_info(entity, {}));
	}

	// run post update
	for (auto& info : animation_infos)
	{
		auto& _animation = info.second;
		const auto player = _animation.player;

		// erase frames out-of-range
		for (int i = 0; i < _animation.frames.size(); i++)
			if (!_animation.frames[i].is_valid(0.45f, 0.2f))
				_animation.frames.erase(_animation.frames.begin() + i);

		// have we already seen this update?
		if (player->GetSimulationTime() == player->GetOldSimulationTime())
			continue;

		animation pBackupRecord = animation(player);
		pBackupRecord.apply(player);

		animation* pPreviousRecord = nullptr;

		if (!_animation.frames.empty() && !_animation.frames.front().dormant &&
			TIME_TO_TICKS(player->GetSimulationTime() - _animation.frames.front().sim_time) <= 17) {
			pPreviousRecord = &_animation.frames.front();
		}

		const auto weapon = player->GetWeapon();

		const bool bShot = weapon
			&& pPreviousRecord && weapon->GetLastShotTime() > pPreviousRecord->sim_time
			&& weapon->GetLastShotTime() <= player->GetSimulationTime();

		if (!bShot) {
			info.second.last_reliable_angle = player->GetEyeAngles();
		}

		auto& pCurrentRecord = _animation.frames.emplace_front(player, info.second.last_reliable_angle);

		_animation.update_animations(&pCurrentRecord, pPreviousRecord);

		memcpy(player->GetAnimOverlays(), pBackupRecord.layers, sizeof(CAnimationLayer) * 13);

		pCurrentRecord.build_server_bones(player);

		pBackupRecord.restore(player);
	}
}


void CMAnimationFix::update_player(IBasePlayer* player, animation* record, animation* previous)
{
	static auto& enable_bone_cache_invalidation = **reinterpret_cast<bool**>(
		reinterpret_cast<uint32_t>((void*)csgo->Utils.FindPatternIDA(GetModuleHandleA(g_Modules[fnv::hash(hs::client_dll::s().c_str())].c_str()),
			hs::bone_cache_validation::s().c_str())) + 2);

	//// make a backup of globals
	const auto backup_frametime = interfaces.global_vars->frametime;
	const auto backup_curtime = interfaces.global_vars->curtime;
	const auto old_flags = player->GetFlagsPtr();

	// get player anim state
	auto state = player->GetPlayerAnimState();

	if (state->m_iLastClientSideAnimationUpdateFramecount == interfaces.global_vars->framecount)
		state->m_iLastClientSideAnimationUpdateFramecount -= 1.f;

	// fixes for networked players
	interfaces.global_vars->frametime = interfaces.global_vars->interval_per_tick;
	interfaces.global_vars->curtime = player->GetSimulationTime();

	if (player->GetAnimOverlay(5)->m_flWeight > 0.0f)
		player->GetFlagsPtr() |= FL_ONGROUND;

	player->GetEFlags() &= ~0x1000;
	player->GetAbsVelocity() = player->GetVelocity();

	player->InvalidatePhysicsRecursive(ANIMATION_CHANGED);//++hh

	// make sure we keep track of the original invalidation state
	const auto old_invalidation = enable_bone_cache_invalidation;

	// notify the other hooks to instruct animations and pvs fix

	csgo->EnableBones = player->GetClientSideAnims() = true;
	resolver->Do(player, record);
	player->UpdateClientSideAnimation();
	csgo->EnableBones = player->GetClientSideAnims() = false;

	player->InvalidatePhysicsRecursive(BOUNDS_CHANGED);//++hh

	player->InvalidatePhysicsRecursive(ANGLES_CHANGED);
	player->InvalidatePhysicsRecursive(ANIMATION_CHANGED);
	player->InvalidatePhysicsRecursive(SEQUENCE_CHANGED);

	// we don't want to enable cache invalidation by accident
	enable_bone_cache_invalidation = old_invalidation;

	// restore globals
	interfaces.global_vars->curtime = backup_curtime;
	interfaces.global_vars->frametime = backup_frametime;

	player->GetFlagsPtr() = old_flags;
}

CMAnimationFix::animation_info* CMAnimationFix::get_animation_info(IBasePlayer* player)
{
	auto info = animation_infos.find(player->GetRefEHandle());

	if (info == animation_infos.end())
		return nullptr;

	return &info->second;
}

/*bool animation::is_valid_extended()
{
	if (!interfaces.engine->GetNetChannelInfo() || !valid)
		return false;

	const auto correct = std::clamp(interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING)
		+ interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING)
		+ calculate_lerp(), 0.f, 0.2f);

	float deltaTime = fabsf(correct - (interfaces.global_vars->curtime - sim_time));
	float ping = 0.2f;
	if (CanDT() && csgo->dt_charged && !CMAntiAim::Get().did_shot)
		ping += TICKS_TO_TIME(8);
	return deltaTime < ping&& deltaTime >= ping - .2f;
}*/
bool animation::is_valid_extended()
{
	if (!interfaces.engine->GetNetChannelInfo() || !valid)
		return false;

	auto correct = interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_OUTGOING) + interfaces.engine->GetNetChannelInfo()->GetLatency(FLOW_INCOMING) + calculate_lerp();

	static auto sv_maxunlag = interfaces.cvars->FindVar("sv_maxunlag");
	correct = std::clamp(correct, 0.0f, sv_maxunlag->GetFloat());

	auto delta_time = fabs(correct - (interfaces.global_vars->curtime - sim_time));
	float ping = sv_maxunlag->GetFloat();
	if (CanDT() && csgo->dt_charged && !CMAntiAim::Get().did_shot && vars.ragebot.double_tap)
		ping += TICKS_TO_TIME(8);
	return delta_time <= sv_maxunlag->GetFloat() && delta_time >= ping - .2f;
}
std::optional<animation*> CMAnimationFix::get_latest_animation(IBasePlayer* player)
{
	const auto pInfo = animation_infos.find(player->GetRefEHandle());
	if (pInfo == animation_infos.end() || pInfo->second.frames.empty()) {
		return std::nullopt;
	}

	animation* first_invalid = nullptr;

	for (auto it = pInfo->second.frames.begin(); it != pInfo->second.frames.end(); it = next(it)) {

		if (!first_invalid)
			first_invalid = &*it;

		if (it->is_valid_extended()) {
			return &*it;
		}
	}

	if (first_invalid)
		return first_invalid;
	else
		return std::nullopt;
}

std::optional<animation*> CMAnimationFix::get_oldest_animation(IBasePlayer* player)
{
	const auto pInfo = animation_infos.find(player->GetRefEHandle());
	if (pInfo == animation_infos.end() || pInfo->second.frames.empty()) {
		return std::nullopt;
	}

	for (auto it = pInfo->second.frames.rbegin(); it != pInfo->second.frames.rend(); it = next(it)) {
		if (it->is_valid_extended()) {
			return &*it;
		}
	}

	return std::nullopt;
}

std::optional<animation*> CMAnimationFix::get_latest_firing_animation(IBasePlayer* player)
{
	const auto info = animation_infos.find(player->GetRefEHandle());

	if (info == animation_infos.end() || info->second.frames.empty())
		return std::nullopt;

	for (auto it = info->second.frames.begin(); it != info->second.frames.end(); it = next(it))
		if ((it)->is_valid_extended() && (it)->didshot)
			return &*it;

	return std::nullopt;

}
//
std::optional<animation*> CMAnimationFix::get_uncrouched_animation(IBasePlayer* player)
{
	const auto info = animation_infos.find(player->GetRefEHandle());

	if (info == animation_infos.end() || info->second.frames.empty())
		return std::nullopt;

	for (auto it = info->second.frames.begin(); it != info->second.frames.end(); it = next(it))
		if (it->is_valid(it->sim_time, it->valid) && fabsf(it->duck) < .0001f && it->didshot)
			return &*it;
		else
			if (it->is_valid(it->sim_time, it->valid) && fabsf(it->duck) < .0001f)
				return &*it;

	return std::nullopt;
}
std::optional<animation*> CMAnimationFix::get_latest_upPitch_animation(IBasePlayer* player)
{
	const auto info = animation_infos.find(player->GetRefEHandle());

	if (info == animation_infos.end() || info->second.frames.empty())
		return std::nullopt;

	for (auto it = info->second.frames.begin(); it != info->second.frames.end(); it = next(it))
		if (it->is_valid(it->sim_time, it->valid) && it->upPitch)
			return &*it;

	return std::nullopt;
}
std::optional<animation*> CMAnimationFix::get_latest_sideways_animation(IBasePlayer* player)
{
	const auto info = animation_infos.find(player->GetRefEHandle());

	if (info == animation_infos.end() || info->second.frames.empty())
		return std::nullopt;

	for (auto it = info->second.frames.begin(); it != info->second.frames.end(); it = next(it))
		if (it->is_valid(it->sim_time, it->valid) && it->sideways)
			return &*it;

	return std::nullopt;
}
void CMAnimationFix::UpdateFakeState()
{
	if ((!interfaces.engine->IsConnected() && !interfaces.engine->IsInGame()) || !csgo->local) {
		return;
	}

	if (!csgo->local->isAlive() || csgo->freeze_period) {
		return;
	}

	static CBaseHandle* selfhandle = nullptr;
	static float spawntime = csgo->local->GetSpawnTime();

	auto alloc = FakeAnimstate == nullptr;
	auto change = !alloc && selfhandle != &csgo->local->GetRefEHandle();
	auto reset = !alloc && !change && csgo->local->GetSpawnTime() != spawntime;

	if (change) {
		memset(&FakeAnimstate, 0, sizeof(FakeAnimstate));
		selfhandle = (CBaseHandle*)&csgo->local->GetRefEHandle();
	}
	if (reset) {
		csgo->local->ResetAnimationState(FakeAnimstate);
		spawntime = csgo->local->GetSpawnTime();
	}

	if (alloc || change) {
		FakeAnimstate = reinterpret_cast<CCSGOPlayerAnimState*>(interfaces.memalloc->Alloc(sizeof(CCSGOPlayerAnimState)));
		if (FakeAnimstate)
			csgo->local->CreateAnimationState(FakeAnimstate);
	}

	if (FakeAnimstate->m_iLastClientSideAnimationUpdateFramecount == interfaces.global_vars->framecount)
		FakeAnimstate->m_iLastClientSideAnimationUpdateFramecount -= 1.f;

	csgo->local->GetEffects() |= 0x8;

	csgo->local->InvalidateBoneCache();

	CAnimationLayer backup_layers[13];
	if (csgo->local->GetOldSimulationTime() != csgo->local->GetSimulationTime())
	{
		std::memcpy(backup_layers, csgo->local->GetAnimOverlays(),
			(sizeof(CAnimationLayer) * csgo->local->GetNumAnimOverlays()));

		csgo->local->UpdateAnimationState(FakeAnimstate, csgo->FakeAngle); // update animstate
		csgo->local->SetAbsAngles(Vector(0, FakeAnimstate->m_flGoalFeetYaw, 0));
		csgo->local->GetAnimOverlay(12)->m_flWeight = FLT_EPSILON;
		csgo->local->SetupBones(csgo->fakematrix, 128, 0x7FF00, interfaces.global_vars->curtime);// setup matrix
		csgo->local->SetupBones(csgo->fakelag_matrix, 128, 0x7FF00, interfaces.global_vars->curtime);// setup matrix
		if (true) {
			for (auto& i : csgo->fakematrix)
			{
				i[0][3] -= csgo->local->GetRenderOrigin().x;
				i[1][3] -= csgo->local->GetRenderOrigin().y;
				i[2][3] -= csgo->local->GetRenderOrigin().z;
			}
		}

		std::memcpy(csgo->local->GetAnimOverlays(), backup_layers,
			(sizeof(CAnimationLayer) * csgo->local->GetNumAnimOverlays()));
	}

	csgo->local->GetEffects() &= ~0x8;
}

void CMAnimationFix::ApplyLocalPlayer() {
	auto animstate = csgo->local->GetPlayerAnimState();
	if (!animstate)
		return;

	const auto backup_frametime = interfaces.global_vars->frametime;
	const auto backup_curtime = interfaces.global_vars->curtime;

	animstate->m_flGoalFeetYaw = csgo->VisualAngle.y;

	if (animstate->m_iLastClientSideAnimationUpdateFramecount == interfaces.global_vars->framecount)
		animstate->m_iLastClientSideAnimationUpdateFramecount -= 1.f;

	// update anim update delta as server build.
	animstate->m_flUpdateTimeDelta = max(0.0f, interfaces.global_vars->curtime - animstate->m_flLastClientSideAnimationUpdateTime);

	interfaces.global_vars->frametime = interfaces.global_vars->interval_per_tick;
	interfaces.global_vars->curtime = csgo->local->GetSimulationTime();

	csgo->local->GetEFlags() &= ~0x1000;
	csgo->local->GetAbsVelocity() = csgo->local->GetVelocity();

	animstate->m_bInHitGroundAnimation = false;

	static float angle = animstate->m_flGoalFeetYaw;

	animstate->m_flFeetYawRate = 0.f;

	CAnimationLayer backup_layers[13];
	if (csgo->local->GetSimulationTime() != csgo->local->GetOldSimulationTime())
	{
		std::memcpy(backup_layers, csgo->local->GetAnimOverlays(),
			(sizeof(CAnimationLayer) * csgo->local->GetNumAnimOverlays()));

		csgo->ShouldUpdate = csgo->local->GetClientSideAnims() = true;
		csgo->local->UpdateAnimationState(animstate, csgo->FakeAngle);
		csgo->local->UpdateClientSideAnimation();
		csgo->ShouldUpdate = csgo->local->GetClientSideAnims() = false;

		angle = animstate->m_flGoalFeetYaw;

		std::memcpy(csgo->local->GetAnimOverlays(), backup_layers,
			(sizeof(CAnimationLayer) * csgo->local->GetNumAnimOverlays()));
	}
	animstate->m_flGoalFeetYaw = angle;
	interfaces.global_vars->curtime = backup_curtime;
	interfaces.global_vars->frametime = backup_frametime;
	//csgo->local->DrawServerHitboxes();
}