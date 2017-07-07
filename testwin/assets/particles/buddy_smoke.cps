particle_system smoke
{
	src = "assets/particles/smoke.png"
	blendmode = add
	gravity = 0,0,0
	velocity = 0,0.14,0
	lifetime = 1.2
	lifetime_variance = 0.2
	colour = 1,1,1,1
	rotation_speed = 0.5
	scale_affector = 1.9
	size = 0.04
	emit_rate = 25
	spawn_radius = 0.1

	forces
	{
		force = -0.7,0.85,0
		//force = -1,-0.2,0
		//force = -1,-0.2,0
		//force = -1,-0.2,0
	}

	affector colour
	{

	}
}