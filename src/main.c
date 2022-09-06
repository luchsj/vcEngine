#include "wm.h"

#include <stdio.h>

int main(int argc, const char* argv[])
{
	wm_window_t* window = wm_create();

	// THIS IS THE MAIN LOOP!
	while (!wm_pump(window))
	{
<<<<<<< HEAD
	//	printf("FRAME!\n");
=======
		int x, y;
		wm_get_mouse_move(window, &x, &y);
		printf("MOUSE mask=%x move=%dx%x\n",
			wm_get_mouse_mask(window),
			x, y);
>>>>>>> eacf95569f1f4cdf48b9420e8591919efd3db7c9
	}

	wm_destroy(window);

	return 0;
}
