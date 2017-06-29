particle_system smoke
{
	src = "assets/particles/smoke.png"
	blendmode = add
	gravity = 0,0,0
	velocity = 0,0.2,0
	lifetime = 2
	lifetime_variance = 0.2
	colour = 1,1,1,1
	rotation_speed = 0.5
	scale_affector = 2.4
	size = 0.01
	emit_rate = 15
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