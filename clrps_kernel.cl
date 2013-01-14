/*
 * BF random number generator
 * source: http://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
 * also works with glsl
 */
float randBF(float2 seed) {
	float rnd_floor;
    return fract(sin(dot(seed, (float2){12.9898, 78.233})) * 43758.5453, &rnd_floor);
}

/*
 * Rock, paper, scissors simulator kernel.
 * logic source:
 * www.gamedev.net/blog/844/entry-2249737-another-cellular-automaton-video/
 *
 */
__kernel void rps(
		__read_only 	image2d_t universe,
		__write_only 	image2d_t output,
		__global		float *random,
						sampler_t sampler) {
						
	float x_unit = 1.0 / WIDTH;
	float y_unit = 1.0 / HEIGHT;
	
	float2	fcoord = (float2){
		get_global_id(0) / (float) WIDTH,
		get_global_id(1) / (float) HEIGHT
		};
	int2	icoord = (int2){get_global_id(0), get_global_id(1)};
	
	float 	current_fstate;
	int		current_istate;
	
	float2 	neighbour_coord;	
	int 	neighbour_state;
	float 	rnd = randBF((float2){
		random[get_global_id(1) * WIDTH + get_global_id(0)],
		random[get_global_id(1) * WIDTH + get_global_id(0) + 1]
		}) * 8;
	
	// Choose neighbour
	if(rnd > 7.0) {
		neighbour_coord = (float2){fcoord.x - x_unit, fcoord.y + y_unit};
	}
	else if(rnd > 6.0) {
		neighbour_coord = (float2){fcoord.x, fcoord.y + y_unit};
	}
	else if(rnd > 5.0) {
		neighbour_coord = (float2){fcoord.x + x_unit, fcoord.y + y_unit};
	}
	else if(rnd > 4.0) {
		neighbour_coord = (float2){fcoord.x - x_unit, fcoord.y};
	}
	else if(rnd > 3.0) {
		neighbour_coord = (float2){fcoord.x + x_unit, fcoord.y};
	}
	else if(rnd > 2.0) {
		neighbour_coord = (float2){fcoord.x - x_unit, fcoord.y - y_unit};
	}
	else if(rnd > 1.0) {
		neighbour_coord = (float2){fcoord.x, fcoord.y - y_unit};
	}
	else {
		neighbour_coord = (float2){fcoord.x + x_unit, fcoord.y - y_unit};
	}
	
	neighbour_state = (int){read_imagef(universe, sampler, neighbour_coord).x * 255};
	
	current_fstate = read_imagef(universe, sampler, fcoord).x;
	current_istate = (int){current_fstate * 255};
	
	// If current cell is empty, try fill it with neighbour's state
	if(current_istate == 0) {
		if(	neighbour_state != 0	&&
			neighbour_state != 10	&&
			neighbour_state != 20	&&
			neighbour_state != 30	) {
			current_istate = neighbour_state - 1;
		}
	}
	
	// If it is rock / red
	else if(current_istate < 20) {
		// If neighbour is paper / green
		if(neighbour_state < 30 && neighbour_state >= 20) {
			current_istate = current_istate - 1;
			// If current cell is eliminated, change it to paper / green
			if(current_istate < 10) {
				current_istate = 29;
			}
		}
	}
	
	// If it is paper / green
	else if(current_istate < 30) {
		// If neighbour is scissors / blue
		if(neighbour_state < 40 && neighbour_state >= 30) {
			current_istate = current_istate - 1;
			// If current cell is eliminated, change it to scissors / blue
			if(current_istate < 20) {
				current_istate = 39;
			}
		}
	}
	
	// If it is scissor / blue
	else if(current_istate < 40) {
		// If neighbour is rock / red
		if(neighbour_state < 20 && neighbour_state >= 10) {
			current_istate = current_istate - 1;
			// If current cell is eliminated, change it to rock / red
			if(current_istate < 30) {
				current_istate = 19;
			}
		}
	}
	
	// Transfor state back to float
	current_fstate = (float){current_istate / 255.0};
	
	// Write out new state, and random seed
	write_imagef(output, icoord, current_fstate);
	random[get_global_id(1) * WIDTH + get_global_id(0)] = rnd;
}