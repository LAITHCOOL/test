#include "animation_system.h"
#include "..\ragebot\aim.h"

void resolver::initialize(player_t* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch)
{
	player = e;
	player_record = record;

	original_goal_feet_yaw = math::normalize_yaw(goal_feet_yaw);
	original_pitch = math::normalize_pitch(pitch);
}

void resolver::reset()
{
	player = nullptr;
	player_record = nullptr;
	
}


float_t get_backward_side(player_t* player)
{
	return math::calculate_angle(globals.local()->m_vecOrigin(), player->m_vecOrigin()).y;
}


Vector GetHitboxPos(player_t* player, matrix3x4_t* mat, int hitbox_id)
{
	if (!player)
		return Vector();

	auto hdr = m_modelinfo()->GetStudioModel(player->GetModel());

	if (!hdr)
		return Vector();

	auto hitbox_set = hdr->pHitboxSet(player->m_nHitboxSet());

	if (!hitbox_set)
		return Vector();

	auto hitbox = hitbox_set->pHitbox(hitbox_id);

	if (!hitbox)
		return Vector();

	Vector min, max;

	math::vector_transform(hitbox->bbmin, mat[hitbox->bone], min);
	math::vector_transform(hitbox->bbmax, mat[hitbox->bone], max);

	return (min + max) * 0.5f;
}

static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
	Vector delta = a - b;
	float delta_length = delta.Length();

	if (delta_length <= min_delta) {
		Vector result;
		if (-min_delta <= delta_length) {
			return a;
		}
		else {
			float iradius = 1.0f / (delta_length + FLT_EPSILON);
			return b - ((delta * iradius) * min_delta);
		}
	}
	else {
		float iradius = 1.0f / (delta_length + FLT_EPSILON);
		return b + ((delta * iradius) * min_delta);
	}
};

float_t MaxYawModificator(player_t* enemy)
{
	auto animstate = enemy->get_animation_state();

	if (!animstate)
		return 0.0f;

	auto speedfactor = math::clamp(animstate->m_flFeetSpeedForwardsOrSideWays, 0.0f, 1.0f);
	auto avg_speedfactor = (animstate->m_flStopToFullRunningFraction * -0.3f - 0.2f) * speedfactor + 1.0f;

	auto duck_amount = animstate->m_fDuckAmount;

	if (duck_amount)
	{
		auto max_velocity = math::clamp(animstate->m_flFeetSpeedUnknownForwardOrSideways, 0.0f, 1.0f);
		auto duck_speed = duck_amount * max_velocity;

		avg_speedfactor += duck_speed * (0.5f - avg_speedfactor);
	}

	return animstate->yaw_desync_adjustment() * avg_speedfactor;
}

float_t GetBackwardYaw(player_t* player) {

	return math::calculate_angle(player->m_vecOrigin(), player->m_vecOrigin()).y;

}

void resolver::resolve_yaw()
{
	player_info_t player_info;

	if (!m_engine()->GetPlayerInfo(player->EntIndex(), &player_info))
		return;


	if (!globals.local()->is_alive() || player->m_iTeamNum() == globals.local()->m_iTeamNum())
		return;


	auto animstate = player->get_animation_state();


	auto choked = abs(TIME_TO_TICKS(player->m_flSimulationTime() - player->m_flOldSimulationTime()) - 1);

	if (!animstate && choked == 0 || !animstate, choked == 0)
		return;

	float new_body_yaw_pose = 0.0f;
	auto m_flCurrentFeetYaw = player->get_animation_state()->m_flCurrentFeetYaw;
	auto m_flGoalFeetYaw = player->get_animation_state()->m_flGoalFeetYaw;
	auto m_flEyeYaw = player->get_animation_state()->m_flEyeYaw;
	float flMaxYawModifier = MaxYawModificator(player);
	float flMinYawModifier = player->get_animation_state()->pad10[512];
	auto anglesy = math::normalize_yaw(player->m_angEyeAngles().y - original_goal_feet_yaw);

	auto valid_lby = true;

	auto speed = player->m_vecVelocity().Length2D();

	float m_lby = player->m_flLowerBodyYawTarget() * 0.574f;

	if (animstate->m_velocity > 0.1f || fabs(animstate->flUpVelocity) > 100.f)
		valid_lby = animstate->m_flTimeSinceStartedMoving < 0.22f;

	auto absangles = player_record->abs_angles.y + player_record->abs_angles.x;


	auto player_stand = player->m_vecVelocity().Length2D();
	player_stand = 0.f;

	auto v58 = *(float*)((uintptr_t)animstate + 0x334) * ((((*(float*)((uintptr_t)animstate + 0x11C)) * -0.30000001) - 0.19999999) * animstate->m_flFeetSpeedForwardsOrSideWays) + 1.0f;
	auto v59 = *(float*)((uintptr_t)animstate + 0x330) * ((((*(float*)((uintptr_t)animstate + 0x11C)) * -0.30000001) - 0.19999999) * animstate->m_flFeetSpeedForwardsOrSideWays) + 1.0f;

	auto angrec = player_record->angles.y;

	///////////////////// [ FLIGHT-FIX ] /////////////////////
	AnimationLayer layers[13];

	if (player_record->flags & MOVETYPE_NOCLIP && player->m_fFlags() & MOVETYPE_NOCLIP)
	{
		if ((int(layers[6].m_flWeight * 1000.f == (int(layers[6].m_flWeight * 1000.f)))))
		{
			animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 0.f);
		}
	}

	///////////////////// [ FLIGHT-FIX ] /////////////////////

	float m_flLastClientSideAnimationUpdateTimeDelta = 0.0f;
	auto trace = false;
	AnimationLayer moveLayers[3][13];
	adjust_data* previous_record;
	memcpy(layers, player->get_animlayers(), sizeof(AnimationLayer) * 15);
	int updateanim;
	bool first_detected = false;
	bool second_detected = false;
	auto detecteddelta = math::AngleDiff(animstate->m_flEyeYaw, animstate->m_flGoalFeetYaw);
	int switch_side;
	auto delta = math::AngleDiff(m_flEyeYaw, m_flGoalFeetYaw);
	auto delta2 = math::normalize_yaw(player->m_angEyeAngles().y - original_goal_feet_yaw);
	auto sidefirst = globals.g.eye_pos.DistTo(player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.first));
	auto sidesecond = globals.g.eye_pos.DistTo(player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.second));
	auto sidezero = globals.g.eye_pos.DistTo(player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.zero));



	if (animstate->m_flFeetSpeedForwardsOrSideWays >= 0.0)
		animstate->m_flFeetSpeedForwardsOrSideWays = fminf(animstate->m_flFeetSpeedForwardsOrSideWays, 1.0);
	else
		animstate->m_flFeetSpeedForwardsOrSideWays = 0.0;

	auto v54 = animstate->m_fDuckAmount;
	auto v55 = ((((*(float*)((uintptr_t)animstate + 0x11C)) * -0.30000001) - 0.19999999) * animstate->m_flFeetSpeedForwardsOrSideWays) + 1.0f;
	if (v54 > 0.0)
	{
		if (animstate->m_flFeetSpeedUnknownForwardOrSideways >= 0.0)
			animstate->m_flFeetSpeedUnknownForwardOrSideways = fminf(animstate->m_flFeetSpeedUnknownForwardOrSideways, 1.0);
		else
			animstate->m_flFeetSpeedUnknownForwardOrSideways = 0.0;

		v55 += ((animstate->m_flFeetSpeedUnknownForwardOrSideways * v54) * (0.5f - v55));
	}

	bool bWasMovingLastUpdate = false;
	bool bJustStartedMovingLastUpdate = false;
	if (player->m_vecVelocity().Length2D() <= 0.0f)
	{
		animstate->m_flTimeSinceStartedMoving = 0.0f;
		bWasMovingLastUpdate = animstate->m_flTimeSinceStoppedMoving <= 0.0f;
		animstate->m_flTimeSinceStoppedMoving += animstate->m_flLastClientSideAnimationUpdateTime;
	}
	else
	{
		animstate->m_flTimeSinceStoppedMoving = 0.0f;
		bJustStartedMovingLastUpdate = animstate->m_flTimeSinceStartedMoving <= 0.0f;
		animstate->m_flTimeSinceStartedMoving = animstate->m_flLastClientSideAnimationUpdateTime + animstate->m_flTimeSinceStartedMoving;
	}
	auto unknown_velocity = *(float*)(uintptr_t(animstate) + 0x2A4);
	if (animstate->m_flFeetSpeedUnknownForwardOrSideways < 1.0f)
	{
		if (animstate->m_flFeetSpeedUnknownForwardOrSideways < 0.5f)
		{
			float velocity = unknown_velocity;
			float delta = animstate->m_flLastClientSideAnimationUpdateTime * 60.0f;
			float new_velocity;
			if ((80.0f - velocity) <= delta)
			{
				if (-delta <= (80.0f - velocity))
					new_velocity = 80.0f;
				else
					new_velocity = velocity - delta;
			}
			else
			{
				new_velocity = velocity + delta;
			}
			unknown_velocity = new_velocity;
		}
	}
	float cycle = (layers->m_flPlaybackRate * animstate->m_flLastClientSideAnimationUpdateTime) + layers->m_flCycle;

	cycle -= (float)(int)cycle;

	if (cycle < 0.0f)
		cycle += 1.0f;

	if (cycle > 1.0f)
		cycle -= 1.0f;

					   /////////////////////// [ ANTI REVERSE AND PIZDABOL DETECT SYSTEM ] ///////////////////////

 
     /////// [ Здарова, реверсная хуила. Что, решил пореверсить? Правильно, у самого то мозгов нету, придумать что-то новое, пастер ебливый, можешь идти нахуй  ] ////////// 
     /////// [ Stop Reversing u Dumb nn kid, u started this cause ure cant try make ur own resolve method, nice iq, u sell? ] ////////// 
     /////// [ Polska Bialo Czerwony. Polak stop fucking pasting this, u retard, Poslka bialo czerwony, ratunku suka, liptonek zielony, kurwa. Milo Ce Poznac  ] ////////// 
     /////// [ Dein deutsches Schwein wartet auf dem Tisch auf dich, mit offener Muschi, bereit, deine Mutter zusammen mit deinem Adoptivvater zu ficken. ] ////////// 


			          /////////////////////// [ ANTI REVERSE AND PIZDABOL DETECT SYSTEM ] ///////////////////////


	auto first_matrix = player_record->matrixes_data.first;
	auto second_matrix = player_record->matrixes_data.second;
	auto central_matrix = player_record->matrixes_data.zero;
	auto leftPose = GetHitboxPos(player, first_matrix, HITBOX_HEAD);
	auto rightPose = GetHitboxPos(player, second_matrix, HITBOX_HEAD);
	auto centralPose = GetHitboxPos(player, central_matrix, HITBOX_HEAD);
	

	auto fire_first = autowall::get().wall_penetration(globals.g.eye_pos, player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.first), player);
	auto fire_second = autowall::get().wall_penetration(globals.g.eye_pos, player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.second), player);
	auto fire_third = autowall::get().wall_penetration(globals.g.eye_pos, player->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.zero), player);

         ///////////////////// [ ANIMLAYERS ] /////////////////////

	if (player_record->flags & FL_ONGROUND || player->m_fFlags() & FL_ONGROUND) {
		int i = player->EntIndex();
		auto matrix_safe = player_record->matrixes_data.zero->GetOrigin()[0] == player_record->matrixes_data.first->GetOrigin()[0] || player_record->matrixes_data.zero->GetOrigin()[0] == player_record->matrixes_data.second->GetOrigin()[0] || player_record->matrixes_data.first->GetOrigin()[0] == player_record->matrixes_data.second->GetOrigin()[0];
		auto m_MaxDesyncDelta = player->get_max_desync_delta(); \
			Vector Direction;
		bool resolve_first_dir = false;
		bool resolve_second_dir = false;

		const auto player_slowwalking = animstate->m_flFeetYawRate >= 0.01f && animstate->m_flFeetYawRate <= 0.8f;
		auto result = player_record->player->sequence_activity(player_record->layers[3].m_nSequence);
		if (player->m_bDucked() || player->m_bDucking())
		{
			if (m_lby < -30.f)
			{
				animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 45.f);
				switch (globals.g.missed_shots[i] > 1)
				{
				case 0:
					animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 45.f);
					break;
				default:
					break;
				}
			}
			else
			{
				animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 45.f);
				switch (globals.g.missed_shots[i] > 1)
				{
				case 0:
					animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 45.f);
					break;
				default:
					break;
				}
			}
		}
		else if (std::fabsf(layers[6].m_flWeight - layers[6].m_flWeight) < 0.1f)
		{
			if (layers[3].m_flWeight == 0.f && result == 979)
			{
				if (result == 979) {
					if (layers[3].m_flCycle != layers[3].m_flCycle) {
						if (layers[3].m_flWeight == 0.0f && layers[3].m_flCycle == 0.0f)
						{

							Vector src3D, dst3D, forward, right, up, src, dst;
							float back_two, right_two, left_two;
							CGameTrace tr;
							CTraceFilter filter;
							filter.pSkip = globals.local();
							/* angle vectors */
							math::angle_vectors(Vector(0, get_backward_side(player), 0), &forward, &right, &up);

							src3D = player->get_shoot_position();
							dst3D = src3D + (forward * 384);

							/* back engine tracers */
							m_trace()->TraceRay(Ray_t(src3D, dst3D), MASK_SHOT, &filter, &tr);
							back_two = (tr.endpos - tr.startpos).Length();

							/* right engine tracers */
							m_trace()->TraceRay(Ray_t(src3D + right * 35, dst3D + right * 35), MASK_SHOT, &filter, &tr);
							right_two = (tr.endpos - tr.startpos).Length();

							/* left engine tracers */
							m_trace()->TraceRay(Ray_t(src3D - right * 35, dst3D - right * 35), MASK_SHOT, &filter, &tr);
							left_two = (tr.endpos - tr.startpos).Length();

							if (fire_first.damage > 0.f && fire_second.damage > 0.f)
							{
								if (left_two > right_two || fire_first.damage > fire_second.damage)
								{
									animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 60.f);
									switch (globals.g.missed_shots[i] > 1)
									{
									case 0:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 60.f);
										break;
									case 1:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 30.f);
										break;
									case 2:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 30.f);
										break;
									default:
										break;
									}
								}
								else
								{
									animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 60.f);
									switch (globals.g.missed_shots[i] > 1)
									{
									case 0:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 60.f);
										break;
									case 1:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 30.f);
										break;
									case 2:
										animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 30.f);
										break;
									default:
										break;
									}
								}
							}
						}
					}
				}
			}
		}
		else if (speed <= 90.f || player_slowwalking)
		{
			for (int i = 1; i < m_globals()->m_maxclients; i++)
			{
				Vector src3D, dst3D, forward, right, up, src, dst;
				float back_two, right_two, left_two;
				CGameTrace tr;
				CTraceFilter filter;
				filter.pSkip = globals.local();
				/* angle vectors */
				math::angle_vectors(Vector(0, get_backward_side(player), 0), &forward, &right, &up);

				src3D = player->get_shoot_position();
				dst3D = src3D + (forward * 384);

				/* back engine tracers */
				m_trace()->TraceRay(Ray_t(src3D, dst3D), MASK_SHOT, &filter, &tr);
				back_two = (tr.endpos - tr.startpos).Length();

				/* right engine tracers */
				m_trace()->TraceRay(Ray_t(src3D + right * 35, dst3D + right * 35), MASK_SHOT, &filter, &tr);
				right_two = (tr.endpos - tr.startpos).Length();

				/* left engine tracers */
				m_trace()->TraceRay(Ray_t(src3D - right * 35, dst3D - right * 35), MASK_SHOT, &filter, &tr);
				left_two = (tr.endpos - tr.startpos).Length();
				/* side detection */

				if (fire_first.damage > 0.0f && fire_second.damage > 0.0f)
				{
					if (left_two > right_two || fire_first.damage > fire_second.damage) {

						animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 60.f);
						switch (globals.g.missed_shots[i] > 1)
						{
						case 0:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 60.f);
							break;
						case 1:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 20.f);
							break;
						case 2:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 20.f);
							break;
						default:
							break;
						}
					}
					else if (left_two < right_two || fire_first.damage < fire_second.damage) {
						animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 60.f);
						switch (globals.g.missed_shots[i] > 1)
						{
						case 0:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 60.f);
							break;
						case 1:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 20.f);
							break;
						case 2:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 20.f);
							break;
						default:
							break;
						}
					}
					else if (left_two == right_two || left_two == back_two || right_two == back_two)
					{
						if (fire_first.damage <= fire_second.damage)
						{
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 20.f);
							switch (globals.g.missed_shots[i] > 1)
							{
							case 0:
								animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 20.f);
								break;
							default:
								break;
							}
						}
						else
						{
							animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw + 20.f);
							switch (globals.g.missed_shots[i] > 1)
							{
							case 0:
								animstate->m_flGoalFeetYaw = math::normalize_yaw(m_flEyeYaw - 20.f);
								break;
							default:
								break;
							}
						}
					}
				}
			}
		}
		else if (player_slowwalking)
		{
			if (player_slowwalking)
			{
				for (int i = 1; i < m_globals()->m_maxclients; i++)
				{
					Vector src3D, dst3D, forward, right, up, src, dst;
					float back_two, right_two, left_two;
					CGameTrace tr;
					CTraceFilter filter;

					filter.pSkip = globals.local();

					/* angle vectors */
					math::angle_vectors(Vector(0, get_backward_side(player), 0), &forward, &right, &up);

					src3D = player->get_shoot_position();
					dst3D = src3D + (forward * 384);

					/* back engine tracers */
					m_trace()->TraceRay(Ray_t(src3D, dst3D), MASK_SHOT, &filter, &tr);
					back_two = (tr.endpos - tr.startpos).Length();

					/* right engine tracers */
					m_trace()->TraceRay(Ray_t(src3D + right * 35, dst3D + right * 35), MASK_SHOT, &filter, &tr);
					right_two = (tr.endpos - tr.startpos).Length();

					/* left engine tracers */
					m_trace()->TraceRay(Ray_t(src3D - right * 35, dst3D - right * 35), MASK_SHOT, &filter, &tr);
					left_two = (tr.endpos - tr.startpos).Length();

					if (fire_first.damage > 0.0f && fire_second.damage > 0.0f)
					{
						if (left_two == right_two || left_two == back_two || right_two == back_two) {
							if (fire_first.damage == fire_second.damage)
							{
								animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 20.f);
								switch (globals.g.missed_shots[i] > 1)
								{
								case 0:
									animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 20.f);
									break;
								default:
									break;
								}
							}
							else
							{
								animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 20.f);
								switch (globals.g.missed_shots[i] > 1)
								{
								case 0:
									animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 20.f);
									break;
								default:
									break;
								}
							}
						}
					}
				}
			}
		}
		else if (int(layers[6].m_flWeight * 1000.0 == (int(layers[6].m_flWeight * 1000.0))))
		{
			int i = player->EntIndex();
			int m_way;
			float first = abs(layers[6].m_flPlaybackRate - moveLayers[0][6].m_flPlaybackRate);
			float second = abs(layers[6].m_flPlaybackRate - moveLayers[2][6].m_flPlaybackRate);
			float third = abs(layers[6].m_flPlaybackRate - moveLayers[1][6].m_flPlaybackRate);
			if (int(first) <= int(second) || int(third) < int(second) || int(second * 1000.0f))
			{
				if (int(first) >= int(third) && int(second) > int(third) && !int(third * 1000.0f))
				{
					animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 58.f);
					switch (globals.g.missed_shots[i] % 2)
					{
						case 0:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 30.f);
							break;
						case 1:
							animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 30.f);
							break;
						default:
							break;
					}
				}
			}
			else
			{
				animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 58.f);
				switch (globals.g.missed_shots[i] % 2)
				{
					case 0:
						animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 30.f);
						break;
					case 1:
						animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 30.f);
						break;
					default:
						break;
				}
			}	
		}	///////////////////// [ ANIMLAYERS ] /////////////////////
	}
}
bool resolver::ent_use_jitter(player_t* player, int* new_side) {   // hello weave

	if (!player->is_alive())
		return false;

	if (player->IsDormant())
		return false;

	static float LastAngle[64];
	static int LastBrute[64];
	static bool Switch[64];
	static float LastUpdateTime[64];

	int i = player->EntIndex();

	float CurrentAngle = player->m_angEyeAngles().y;
	if (math::AngleDiff(CurrentAngle,LastAngle[i])) {
		Switch[i] = !Switch[i];
		LastAngle[i] = CurrentAngle;
		*new_side = Switch[i] ? -1 : 1;
		LastBrute[i] = *new_side;
		LastUpdateTime[i] = m_globals()->m_curtime;
		return true;
	}
	else {
		if (fabsf(LastUpdateTime[i] - m_globals()->m_curtime >= TICKS_TO_TIME(17))
			|| player->m_flSimulationTime() != player->m_flOldSimulationTime()) {
			LastAngle[i] = CurrentAngle;
		}
		*new_side = LastBrute[i];
	}
	return false;
}

float_t resolver::resolve_pitch()
{
		return original_pitch;  // хуитч
}

