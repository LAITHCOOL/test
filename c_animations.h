#pragma once
#include "../c_hooks.h"
#include <unordered_map>
#include <optional>
#include <deque>

struct animation
{
	animation() = default;
	explicit animation(IBasePlayer* player);
	explicit animation(IBasePlayer* player, Vector last_reliable_angle);
	void restore(IBasePlayer* player) const;
	void apply(IBasePlayer* player) const;
	void build_server_bones(IBasePlayer* player);
	bool is_valid(float range, float max_unlag);
	bool is_valid_extended();

	IBasePlayer* player{};
	int32_t index{};

	bool valid{}, has_anim_state{};
	alignas(16) matrix bones[128];

	bool dormant{};
	Vector velocity;
	Vector origin;
	matrix* bone_cache;
	Vector abs_origin;
	Vector obb_mins;
	Vector obb_maxs;
	CAnimationLayer layers[13];
	CAnimationLayer				m_ResolverLayers[3][15];//
	std::array<float, 24> poses;
	CAnimationLayer* anim_layer_t;
	CCSGOPlayerAnimState* anim_state;
	float anim_time{};
	float sim_time{};
	float interp_time{};
	float duck{};
	float lby{};
	float last_shot_time{};
	Vector last_reliable_angle{};
	int upPitch{};
	Vector eye_angles;
	Vector abs_ang;
	int flags{};
	int eflags{};
	int effects{};
	float m_flFeetCycle{};
	float m_flFeetYawRate{};
	int sideways{};
	int lag{};
	bool didshot;
	string resolver;
};


class CMAnimationFix
{
public:

private:
	struct animation_info {
		animation_info(IBasePlayer* player, std::deque<animation> animations)
			: player(player), frames(std::move(animations)), last_spawn_time(0) { }

		animation_info() {

		}

		void update_animations(animation* to, animation* from);

		IBasePlayer* player{};
		std::deque<animation> frames;

		// last time this player spawned
		float last_spawn_time;
		float brute_yaw;
		float goal_feet_yaw;
		Vector last_reliable_angle;
	};

	unordered_map<CBaseHandle, animation_info> animation_infos;

public:
	void ApplyLocalPlayer();
	void UpdatePlayers();
	void UpdateFakeState();

	CCSGOPlayerAnimState* RealAnimstate = nullptr;
	CCSGOPlayerAnimState* FakeAnimstate = nullptr;
	CBaseHandle* selfhandle;
	void update_player(IBasePlayer* player, animation* record, animation* previous);


	animation_info* get_animation_info(IBasePlayer* player);
	std::optional<animation*> get_latest_animation(IBasePlayer* player);
	std::optional<animation*> get_oldest_animation(IBasePlayer* player);
	std::optional<std::pair<animation*, animation*>> get_intermediate_animations(IBasePlayer* player, float range = 1.f);
	std::vector<animation*> get_valid_animations(IBasePlayer* player, float range = 1.f);
	std::optional<animation*> get_latest_firing_animation(IBasePlayer* player);
	std::optional<animation*> get_uncrouched_animation(IBasePlayer* player);
	std::optional<animation*> get_latest_upPitch_animation(IBasePlayer* player);
	std::optional<animation*> get_latest_sideways_animation(IBasePlayer* player);
};
extern CMAnimationFix* g_Animfix;