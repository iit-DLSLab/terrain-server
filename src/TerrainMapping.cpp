#include <terrain_server/TerrainMapping.h>


namespace terrain_server
{

TerrainMapping::TerrainMapping() :
		is_added_feature_(false), is_added_search_area_(false),
		interest_radius_x_(std::numeric_limits<double>::max()),
		interest_radius_y_(std::numeric_limits<double>::max()),
		using_cloud_mean_(false), depth_(16)
{
	// Default neighboring area
	setNeighboringArea(-2, 2, -2, 2, -2, 2);
	terrain_info_.height_map.reset(new std::map<dwl::Vertex,double>);
}


TerrainMapping::~TerrainMapping()
{
	if (is_added_feature_) {
	for (std::vector<dwl::environment::Feature*>::iterator i = features_.begin();
			i != features_.end(); i++)
		delete *i;
	}
}


void TerrainMapping::addFeature(dwl::environment::Feature* feature)
{
	double weight;
	feature->getWeight(weight);
	printf(GREEN_ "Adding the %s feature with a weight of %f\n" COLOR_RESET,
			feature->getName().c_str(), weight);
	features_.push_back(feature);
	is_added_feature_ = true;
}


void TerrainMapping::removeFeature(std::string feature_name)
{
	int num_feature = features_.size();
	for (int i = 0; i < num_feature; i++) {
		if (feature_name == features_[i]->getName().c_str()) {
			printf(GREEN_ "Removing the %s feature\n" COLOR_RESET,
					features_[i]->getName().c_str());
			features_.erase(features_.begin() + i);

			return;
		}
		else if (i == num_feature - 1) {
			printf(YELLOW_ "Could not remove the %s feature\n" COLOR_RESET,
					feature_name.c_str());
		}
	}
}


void TerrainMapping::compute(octomap::OcTree* octomap,
							 const Eigen::Vector4d& robot_state)
{
	if (!is_added_search_area_) {
		printf(YELLOW_ "Warning: adding a default search area \n" COLOR_RESET);
		// Adding a default search area
		addSearchArea(1.5, 4.0, -1.25, 1.25, -0.8, -0.2, 0.04);

		is_added_search_area_ = true;
	}

	if (terrain_information_) {
		// Removing the points that doesn't belong to the interest area
		Eigen::Vector3d robot_2dpose; // (x,y,yaw)
		robot_2dpose(0) = robot_state(0);
		robot_2dpose(1) = robot_state(1);
		robot_2dpose(2) = robot_state(3);
		removeTerrainOutsideInterestRegion(robot_2dpose);
	}


	// Computing terrain map for several search areas
	double yaw = robot_state(3);
	unsigned int area_size = search_areas_.size();
	for (unsigned int n = 0; n < area_size; n++) {
		// Computing the boundary of the gridmap
		Eigen::Vector2d boundary_min, boundary_max;

		boundary_min(0) = search_areas_[n].min_x + robot_state(0);
		boundary_min(1) = search_areas_[n].min_y + robot_state(1);
		boundary_max(0) = search_areas_[n].max_x + robot_state(0);
		boundary_max(1) = search_areas_[n].max_y + robot_state(1);

		double resolution = search_areas_[n].resolution;
		for (double y = boundary_min(1); y <= boundary_max(1); y += resolution) {
			for (double x = boundary_min(0); x <= boundary_max(0); x += resolution) {
				// Computing the rotated coordinate of the point inside the search area
				double xr = (x - robot_state(0)) * cos(yaw) -
							(y - robot_state(1)) * sin(yaw) + robot_state(0);
				double yr = (x - robot_state(0)) * sin(yaw) +
							(y - robot_state(1)) * cos(yaw) + robot_state(1);

				// Checking if the cell belongs to dimensions of the map,
				// and also getting the key of this cell
				double z = search_areas_[n].max_z + robot_state(2);
				octomap::OcTreeKey init_key;
				if (!octomap->coordToKeyChecked(xr, yr, z, depth_, init_key)) {
					printf(RED_ "Cell out of bounds\n" COLOR_RESET);

					return;
				}

				// Finding the cell of the surface
				int r = 0;
				octomap::OcTreeNode* heightmap_node =
						octomap->search(init_key, depth_);
				while (z >= search_areas_[n].min_z + robot_state(2)) {
					octomap::OcTreeKey heightmap_key;
					heightmap_key[0] = init_key[0];
					heightmap_key[1] = init_key[1];
					heightmap_key[2] = init_key[2] - r;

					heightmap_node = octomap->search(heightmap_key, depth_);
					octomap::point3d height_point =
							octomap->keyToCoord(heightmap_key, depth_);
					z = height_point(2);
					if (heightmap_node) {
						// Computation of the heightmap
						if (octomap->isNodeOccupied(heightmap_node)) {
							// Getting position of the occupied cell
							dwl::Key cell_key;
							Eigen::Vector3d cell_position;
							cell_position(0) = height_point(0);
							cell_position(1) = height_point(1);
							cell_position(2) = height_point(2);
							space_discretization_.coordToKeyChecked(cell_key,
																	cell_position);

							dwl::Vertex vertex_id;
							space_discretization_.keyToVertex(vertex_id, cell_key, true);
							if (!terrain_information_)
								addCellToTerrainHeightMap(vertex_id,
														  (double) cell_position(2));
							else {
								bool new_status = true;
								std::map<dwl::Vertex,double>::iterator height_it =
										terrain_heightmap_.find(vertex_id);
								if (height_it != terrain_heightmap_.end()) {
									// Evaluating if it changed status (height)
									unsigned short int old_key_z;
									space_discretization_.coordToKey(old_key_z,
																	 height_it->second,
																	 false);
									if (old_key_z != cell_key.z) {
										removeCellToTerrainMap(vertex_id);
										removeCellToTerrainHeightMap(vertex_id);
									} else
										new_status = false;
								}

								if (new_status)
									addCellToTerrainHeightMap(vertex_id,
															  (double) cell_position(2));
							}
							break;
						}
					}
					r++;
				}
			}
		}
	}

	// Setting the terrain information
	*terrain_info_.height_map = terrain_heightmap_;
	terrain_info_.resolution = space_discretization_.getEnvironmentResolution(true);
	terrain_info_.min_height = min_height_;

	// Computing the terrain map
	for (std::map<dwl::Vertex, double>::iterator terrain_iter = terrain_heightmap_.begin();
			terrain_iter != terrain_heightmap_.end();
			terrain_iter++)
	{
		octomap::OcTreeKey heightmap_key;
		dwl::Vertex vertex_id = terrain_iter->first;
		Eigen::Vector2d xy_coord;
		space_discretization_.vertexToCoord(xy_coord, vertex_id);

		double height = terrain_iter->second;
		octomap::point3d terrain_point;
		terrain_point(0) = xy_coord(0);
		terrain_point(1) = xy_coord(1);
		terrain_point(2) = height;
		heightmap_key = octomap->coordToKey(terrain_point, depth_);

		if (!terrain_information_)
			computeTerrainData(octomap, heightmap_key);
		else {
			bool new_status = true;
			std::map<dwl::Vertex,dwl::TerrainCell>::iterator terrain_it =
					terrain_map_.find(vertex_id);
			if (terrain_it != terrain_map_.end()) {
				// Evaluating if it's changed status (height)
				dwl::TerrainCell terrain_cell = terrain_it->second;

				if (terrain_cell.key.z != heightmap_key[2]) {
					removeCellToTerrainMap(vertex_id);
					removeCellToTerrainHeightMap(vertex_id);
				} else
					new_status = true;//false;
			}

			if (new_status)
				computeTerrainData(octomap, heightmap_key);
		}
	}

	terrain_information_ = true;
}


void TerrainMapping::computeTerrainData(octomap::OcTree* octomap,
										const octomap::OcTreeKey& heightmap_key)
{
	std::vector<Eigen::Vector3f> neighbors_position;
	octomap::OcTreeNode* heightmap_node = octomap->search(heightmap_key, depth_);

	// Adding to the cloud the point of interest
	Eigen::Vector3f heightmap_position;
	octomap::point3d heightmap_point = octomap->keyToCoord(heightmap_key, depth_);
	heightmap_position(0) = heightmap_point(0);
	heightmap_position(1) = heightmap_point(1);
	heightmap_position(2) = heightmap_point(2);
	neighbors_position.push_back(heightmap_position);

	// Iterates over the 8 neighboring sets
	octomap::OcTreeKey neighbor_key;
	octomap::OcTreeNode* neighbor_node = heightmap_node;
	bool is_there_neighboring = false;
	for (int i = neighboring_area_.min_z; i < neighboring_area_.max_z + 1; i++) {
		for (int j = neighboring_area_.min_y; j < neighboring_area_.max_y + 1; j++) {
			for (int k = neighboring_area_.min_x; k < neighboring_area_.max_x + 1; k++) {
				neighbor_key[0] = heightmap_key[0] + k;
				neighbor_key[1] = heightmap_key[1] + j;
				neighbor_key[2] = heightmap_key[2] + i;
				neighbor_node = octomap->search(neighbor_key, depth_);

				if (neighbor_node) {
					Eigen::Vector3f neighbor_position;
					octomap::point3d neighbor_point;
					if (octomap->isNodeOccupied(neighbor_node)) {
						neighbor_point = octomap->keyToCoord(neighbor_key, depth_);
						neighbor_position(0) = neighbor_point(0);
						neighbor_position(1) = neighbor_point(1);
						neighbor_position(2) = neighbor_point(2);
						neighbors_position.push_back(neighbor_position);

						is_there_neighboring = true;
					}
				}
			}
		}
	}

	if (is_there_neighboring) {
		// Computing terrain info
		EIGEN_ALIGN16 Eigen::Matrix3d covariance_matrix;
		if (neighbors_position.size() < 3 ||
				dwl::math::computeMeanAndCovarianceMatrix(terrain_info_.position,
														  covariance_matrix,
														  neighbors_position) == 0)
			return;

		if (!using_cloud_mean_) {
			terrain_info_.position(0) = neighbors_position[0](0);
			terrain_info_.position(1) = neighbors_position[0](1);
			terrain_info_.position(2) = neighbors_position[0](2);
		}

		dwl::math::solvePlaneParameters(terrain_info_.surface_normal,
									    terrain_info_.curvature,
								   covariance_matrix);
	}

	// Computing the cost
	if (is_added_feature_) {
		double cost_value, weight, total_cost = 0;
		unsigned int num_feature = features_.size();
		for (unsigned int i = 0; i < num_feature; i++) {
			features_[i]->computeCost(cost_value, terrain_info_);
			features_[i]->getWeight(weight);
			total_cost += weight * cost_value;
		}

		dwl::TerrainCell cell;
		setTerrainCell(cell,
					   total_cost,
					   (double) heightmap_position(dwl::rbd::Z),
					   terrain_info_);
		addCellToTerrainMap(cell);
	} else {
		printf(YELLOW_ "Could not computed the cost of the features because it"
				" is necessary to add at least one\n" COLOR_RESET);
	}
}


void TerrainMapping::removeTerrainOutsideInterestRegion(const Eigen::Vector3d& robot_state)
{
	// Getting the orientation of the body
	double yaw = robot_state(2);

	for (std::map<dwl::Vertex,dwl::TerrainCell>::iterator vertex_iter = terrain_map_.begin();
			vertex_iter != terrain_map_.end();
			vertex_iter++)
	{
		dwl::Vertex v = vertex_iter->first;
		Eigen::Vector2d point;
		space_discretization_.vertexToCoord(point, v);

		double xc = point(0) - robot_state(0);
		double yc = point(1) - robot_state(1);
		if (xc * cos(yaw) + yc * sin(yaw) >= 0.0) {
			if (pow(xc * cos(yaw) + yc * sin(yaw), 2) / pow(interest_radius_y_, 2) +
					pow(xc * sin(yaw) - yc * cos(yaw), 2) / pow(interest_radius_x_, 2) > 1) {
				terrain_map_.erase(v);
				terrain_heightmap_.erase(v);
			}
		} else {
			if (pow(xc, 2) + pow(yc, 2) > pow(interest_radius_x_, 2)) {
				terrain_map_.erase(v);
				terrain_heightmap_.erase(v);
			}
		}
	}
}


void TerrainMapping::setInterestRegion(double radius_x,
									   double radius_y)
{
	interest_radius_x_ = radius_x;
	interest_radius_y_ = radius_y;
}


void TerrainMapping::addSearchArea(double min_x, double max_x,
								   double min_y, double max_y,
								   double min_z, double max_z,
								   double grid_resolution)
{
	dwl::SearchArea search_area;
	search_area.min_x = min_x;
	search_area.max_x = max_x;
	search_area.min_y = min_y;
	search_area.max_y = max_y;
	search_area.min_z = min_z;
	search_area.max_z = max_z;
	search_area.resolution = grid_resolution;

	search_areas_.push_back(search_area);

	if (!is_added_search_area_ ||
			grid_resolution < space_discretization_.getEnvironmentResolution(true)) {
		space_discretization_.setEnvironmentResolution(grid_resolution, true);
		space_discretization_.setStateResolution(grid_resolution);
	}

	is_added_search_area_ = true;
}


void TerrainMapping::setNeighboringArea(int back_neighbors, int front_neighbors,
										int left_neighbors, int right_neighbors,
										int bottom_neighbors, int top_neighbors)
{
	neighboring_area_.min_x = back_neighbors;
	neighboring_area_.max_x = front_neighbors;
	neighboring_area_.min_y = left_neighbors;
	neighboring_area_.max_y = right_neighbors;
	neighboring_area_.min_z = bottom_neighbors;
	neighboring_area_.max_z = top_neighbors;
}

} //@namepace terrain_server
