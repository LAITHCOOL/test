// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "animation_system.h"
#include "..\ragebot\aim.h"

float resolver::get_angle(player_t* player) {
	return math::NormalizedAngle(player->m_angEyeAngles().y);
}

float resolver::get_backward_yaw(player_t* player) {
	return math::calculate_angle(g_ctx.local()->m_vecOrigin(), player->m_vecOrigin()).y;
}

float resolver::get_forward_yaw(player_t* player) {
	return math::NormalizedAngle(get_backward_yaw(player) - 180.f);
}

bool CanSeeHitbox(player_t* entity, int HITBOX)
{
	return g_ctx.local()->CanSeePlayer(entity, entity->hitbox_position(HITBOX));
}

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

	safe_matrix_shot = false;
	freestand_side = false;

	was_first_bruteforce = false;
	was_second_bruteforce = false;

	original_goal_feet_yaw = 0.0f;
	original_pitch = 0.0f;
}

bool resolver::desync_detect()
{
	if (!player->is_alive())
		return false;
	if (~player->m_fFlags() & FL_ONGROUND)
		return false;
	if (~player->m_iTeamNum() == g_ctx.local()->m_iTeamNum())
		return false;

	return true;
}

bool resolver::Saw(player_t* entity)
{
	if (!(CanSeeHitbox(entity, HITBOX_HEAD) && CanSeeHitbox(entity, HITBOX_LEFT_FOOT) && CanSeeHitbox(entity, HITBOX_RIGHT_FOOT))
		|| (CanSeeHitbox(entity, HITBOX_HEAD) && CanSeeHitbox(entity, HITBOX_LEFT_FOOT) && CanSeeHitbox(entity, HITBOX_RIGHT_FOOT)))
		return false;

	return true;
}

resolver_side resolver::TraceSide(player_t* entity)
{
	auto first_visible = util::visible(g_ctx.globals.eye_pos, entity->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.first), player, g_ctx.local());
	auto second_visible = util::visible(g_ctx.globals.eye_pos, entity->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.second), player, g_ctx.local());
	auto main_visible = util::visible(g_ctx.globals.eye_pos, entity->hitbox_position_matrix(HITBOX_HEAD, player_record->matrixes_data.main), player, g_ctx.local());
	if (main_visible)
	{
		if (first_visible)
			return RESOLVER_SECOND;
		else if (second_visible)
			return  RESOLVER_FIRST;
	}
	else
	{
		if (first_visible)
			return  RESOLVER_FIRST;
		else if (second_visible)
			return  RESOLVER_SECOND;
	}

	return RESOLVER_ORIGINAL;
}

void resolver::antifreestand()
{
	player_info_t player_info;

	auto animstate = player->get_animation_state();

	if (!m_engine()->GetPlayerInfo(player->EntIndex(), &player_info) || !animstate)
		return;

	float angToLocal = math::calculate_angle(g_ctx.local()->m_vecOrigin(), player->m_vecOrigin()).y;

	Vector ViewPoint = g_ctx.local()->m_vecOrigin() + Vector(0, 0, 90);

	Vector2D Side1 = { (45 * sin(DEG2RAD(angToLocal))),(45 * cos(DEG2RAD(angToLocal))) };
	Vector2D Side2 = { (45 * sin(DEG2RAD(angToLocal + 180))) ,(45 * cos(DEG2RAD(angToLocal + 180))) };

	Vector2D Side3 = { (60 * sin(DEG2RAD(angToLocal))),(60 * cos(DEG2RAD(angToLocal))) };
	Vector2D Side4 = { (60 * sin(DEG2RAD(angToLocal + 180))) ,(60 * cos(DEG2RAD(angToLocal + 180))) };

	Vector Origin = player->m_vecOrigin();

	Vector2D OriginLeftRight[] = { Vector2D(Side1.x, Side1.y), Vector2D(Side2.x, Side2.y) };

	Vector2D OriginLeftRightLocal[] = { Vector2D(Side3.x, Side3.y), Vector2D(Side4.x, Side4.y) };

	for (int side = 0; side < 2; side++)
	{
		Vector OriginAutowall = { Origin.x + OriginLeftRight[side].x,  Origin.y - OriginLeftRight[side].y , Origin.z + 90 };
		Vector ViewPointAutowall = { ViewPoint.x + OriginLeftRightLocal[side].x,  ViewPoint.y - OriginLeftRightLocal[side].y , ViewPoint.z };

		if (autowall::get().can_hit_floating_point(OriginAutowall, ViewPoint))
		{
			if (side == 0)
			{
				freestand_side = true;
			}
			else if (side == 1)
			{
				freestand_side = false;
			}
		}
		else
		{
			for (int sidealternative = 0; sidealternative < 2; sidealternative++)
			{
				Vector ViewPointAutowallalternative = { Origin.x + OriginLeftRight[sidealternative].x,  Origin.y - OriginLeftRight[sidealternative].y , Origin.z + 90 };

				if (autowall::get().can_hit_floating_point(ViewPointAutowallalternative, ViewPointAutowall))
				{
					if (sidealternative == 0)
					{
						freestand_side = true;
					}
					else if (sidealternative == 1)
					{
						freestand_side = false;
					}
				}
			}
		}
	}

	// fix
}

void resolver::resolve_yaw()
{
	player_info_t player_info;

	auto animstate = player->get_animation_state();

	if (!m_engine()->GetPlayerInfo(player->EntIndex(), &player_info))
		return;

	safe_matrix_shot = animstate->m_velocity > 180.0f;

	if (!animstate || !desync_detect() || !g_cfg.ragebot.enable)
	{
		player_record->side = RESOLVER_ORIGINAL;
		return;
	}

	//on the stairs or noclip antiaims dont' work
	if (player->get_move_type() == MOVETYPE_LADDER || player->get_move_type() == MOVETYPE_NOCLIP)
	{
		player_record->side = RESOLVER_ORIGINAL;
		return;
	}

	auto delta = math::normalize_yaw(player->m_angEyeAngles().y - original_goal_feet_yaw);
	bool forward = fabsf(math::normalize_yaw(get_angle(player) - get_forward_yaw(player))) < 90.f;

	const float low_delta = math::normalize_yaw(player->m_angEyeAngles().y * 0.5f);

	auto first_side = low_delta ? RESOLVER_LOW_FIRST : RESOLVER_FIRST;
	auto second_side = low_delta ? RESOLVER_LOW_SECOND : RESOLVER_SECOND;

	if (player->m_vecVelocity().Length2D() > 50)
		records.move_lby[player->EntIndex()] = animstate->m_flGoalFeetYaw;

	if (fabs(original_pitch) > 85.0f)
		fake = true;
	else if (!fake)
	{
		player_record->side = RESOLVER_ORIGINAL;
		return;
	}

	if (player->m_vecVelocity().Length2D() > 235 || ~player->m_fFlags() & FL_ONGROUND)
		player_record->side = RESOLVER_ORIGINAL;
	else if (g_ctx.globals.missed_shots[player->EntIndex()] == 0)
		player_record->side = RESOLVER_SECOND;
	else if (Saw(player))
	{
		if (TraceSide(player) != RESOLVER_ORIGINAL)
			player_record->side = TraceSide(player);
		player_record->type = TRACE;
	}

	auto valid_lby = true;

	if (animstate->m_velocity > 0.1f || fabs(animstate->flUpVelocity) > 100.f)
		valid_lby = animstate->m_flTimeSinceStartedMoving < 0.22f;

	if (fabs(delta) > 35.0f && valid_lby)
	{
		if (player->m_fFlags() & FL_ONGROUND || player->m_vecVelocity().Length2D() <= 2.0f)
		{
			if (player->sequence_activity(player_record->layers[3].m_nSequence) == 979)
			{
				if (g_ctx.globals.missed_shots[player->EntIndex()])
					delta = -delta;
			}
			else
			{
				player_record->type = LBY;
				player_record->side = delta > 0 ? first_side : second_side;
			}
		}
	}
	else
	{
		if (!valid_lby && !(int(player_record->layers[12].m_flWeight * 1000.f)) && static_cast<int>(player_record->layers[6].m_flWeight * 1000.f) == static_cast<int>(player_record->previous_layers[6].m_flWeight * 1000.f))
		{
			float delta_first = abs(player_record->layers[6].m_flPlaybackRate - player_record->resolver_layers[1][6].m_flPlaybackRate);
			float delta_second = abs(player_record->layers[6].m_flPlaybackRate - player_record->resolver_layers[2][6].m_flPlaybackRate);
			float delta_third = abs(player_record->layers[6].m_flPlaybackRate - player_record->resolver_layers[0][6].m_flPlaybackRate);

			if (delta_first < delta_second || delta_third <= delta_second || (delta_second * 1000.f))
			{
				if (delta_first >= delta_third && delta_second > delta_third && !(delta_third * 1000.f))
				{
					player_record->type = ANIMATION;
					player_record->side = first_side;
				}
				else
				{
					if (forward)
					{
						player_record->type = DIRECTIONAL;
						player_record->side = freestand_side ? second_side : first_side;
					}
					else
					{
						player_record->type = DIRECTIONAL;
						player_record->side = freestand_side ? first_side : second_side;
					}
				}
			}
			else
			{
				player_record->type = ANIMATION;
				player_record->side = second_side;
			}
		}
		else
		{
			if (forward)
			{
				player_record->type = DIRECTIONAL;
				player_record->side = freestand_side ? second_side : first_side;
			}
			else
			{
				player_record->type = DIRECTIONAL;
				player_record->side = freestand_side ? first_side : second_side;
			}
		}
	}

	side = (int)player_record->side == 2 || (int)player_record->side == 4;

	if (g_ctx.globals.missed_shots[player->EntIndex()] >= 2 || g_ctx.globals.missed_shots[player->EntIndex()] && aim::get().last_target[player->EntIndex()].record.type != LBY && aim::get().last_target[player->EntIndex()].record.type != JITTER)
	{
		switch (last_side)
		{
		case RESOLVER_ORIGINAL:
			g_ctx.globals.missed_shots[player->EntIndex()] = 0;
			return;
		case RESOLVER_ZERO:
			player_record->type = BRUTEFORCE;
			player_record->side = RESOLVER_LOW_FIRST;

			was_first_bruteforce = false;
			was_second_bruteforce = false;
			return;
		case RESOLVER_FIRST:
			player_record->type = BRUTEFORCE;
			player_record->side = was_second_bruteforce ? RESOLVER_ZERO : RESOLVER_SECOND;

			was_first_bruteforce = true;
			return;
		case RESOLVER_SECOND:
			player_record->type = BRUTEFORCE;
			player_record->side = was_first_bruteforce ? RESOLVER_ZERO : RESOLVER_FIRST;

			was_second_bruteforce = true;
			return;
		case RESOLVER_LOW_FIRST:
			player_record->type = BRUTEFORCE;
			player_record->side = RESOLVER_LOW_SECOND;
			return;
		case RESOLVER_LOW_SECOND:
			player_record->type = BRUTEFORCE;
			player_record->side = RESOLVER_FIRST;
			return;
		}
	}
}

float resolver::resolve_pitch()
{
	return original_pitch;
}