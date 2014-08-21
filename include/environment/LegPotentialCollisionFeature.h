#ifndef DWL_LegPotentialCollisionFeature_H
#define DWL_LegPotentialCollisionFeature_H

#include <environment/Feature.h>
#include <utils/utils.h>


namespace dwl
{

namespace environment
{

/**
 * @class LegPotentialCollisionFeature
 * @brief Class for computing the reward value of the leg potential collision feature
 */
class LegPotentialCollisionFeature : public Feature
{
	public:
		/** @brief Constructor function */
		LegPotentialCollisionFeature();

		/** @brief Destructor function */
		~LegPotentialCollisionFeature();

		/**
		 * @brief Compute the reward value given a robot and terrain information
		 * @param double& reward_value Reward value
		 * @param dwl::environment::RobotAndTerrain info Information of the robot and terrain
		 */
		void computeReward(double& reward_value, RobotAndTerrain info);

	private:

};

} //@namespace environment
} //@namespace dwl

#endif