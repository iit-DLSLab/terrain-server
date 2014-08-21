#include <environment/LegPotentialCollisionFeature.h>


namespace dwl
{

namespace environment
{

LegPotentialCollisionFeature::LegPotentialCollisionFeature()
{
	gain_ = 50;
	name_ = "Maximum Height";
}


LegPotentialCollisionFeature::~LegPotentialCollisionFeature()
{

}


void LegPotentialCollisionFeature::computeReward(double& reward_value, RobotAndTerrain info)
{
	// Initilization of the reward value
	reward_value = 0;

	// Setting the resolution of the terrain
	space_discretization_.setEnvironmentResolution(info.resolution, true);

	// Getting the current pose
	Eigen::Vector2d position = info.pose.position;
	double yaw = info.pose.orientation;

	for (int leg = 0; leg < robot_->getNumberOfLegs(); leg++) {
		// Getting the leg area
		SearchArea leg_area = robot_->getLegWorkAreas()[leg];

		Eigen::Vector2d boundary_min, boundary_max;
		boundary_min(0) = leg_area.min_x + position(0);
		boundary_min(1) = leg_area.min_y + position(1);
		boundary_max(0) = leg_area.max_x + position(0);
		boundary_max(1) = leg_area.max_y + position(1);

		// Computing the maximum and minimun height around the leg area
		double max_height = -std::numeric_limits<double>::max();
		double min_height = std::numeric_limits<double>::max();
		double mean_height = 0;
		int counter = 0;
		bool is_there_height_values = false;
		for (double y = boundary_min(1); y < boundary_max(1); y += leg_area.grid_resolution) {
			for (double x = boundary_min(0); x < boundary_max(0); x += leg_area.grid_resolution) {
				Eigen::Vector2d coord;
				coord(0) = (x - position(0)) * cos(yaw) - (y - position(1)) * sin(yaw) + position(0);
				coord(1) = (x - position(0)) * sin(yaw) + (y - position(1)) * cos(yaw) + position(1);
				Vertex vertex_2d;
				space_discretization_.coordToVertex(vertex_2d, coord);

				double height;
				if (info.height_map.find(vertex_2d)->first == vertex_2d) {
					height = info.height_map.find(vertex_2d)->second;

					// Updating the maximum height
					if (height > max_height)
						max_height = height;

					// Updating the minimum height
					if (height < min_height)
						min_height = height;

					mean_height += height;
					counter++;

					is_there_height_values = true;
				}
			}
		}

		if (is_there_height_values) {
			mean_height /= counter;
			double max_diff_height = max_height - mean_height;//min_height;
			reward_value -= gain_ * max_diff_height;
		} else
			reward_value -= 0;
	}
}

} //@namespace environment
} //@namespace dwl