particle_system smoke
{
	src = "assets/particles/smoke.png"
	blendmode = add
	gravity = 0,1,0
	velocity = 0,0.5,0
	lifetime = 2.5
	colour = 1,1,1,1
	rotation_speed = 0.5
	size = 0.005
	emit_rate = 10
	spawn_radius = 0

	forces
	{

	}

	affector colour
	{

	}

	affector size
	{
		scale = 1.3
	}
}