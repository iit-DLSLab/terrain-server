#ifndef DWL__ENVIRONMENT__SLOPE_FEATURE__H
#define DWL__ENVIRONMENT__SLOPE_FEATURE__H

#include <environment/Feature.h>


namespace dwl
{

namespace environment
{

/**
 * @class SlopeFeature
 * @brief Class for computing the reward value of the slope feature
 */
class SlopeFeature : public Feature
{
	public:
		/** @brief Constructor function */
		SlopeFeature();

		/** @brief Destructor function */
		~SlopeFeature();

		/**
		 * @brief Compute the reward value given a terrain information
		 * @param double& Reward value
		 * @param Terrain Information of the terrain
		 */
		void computeReward(double& reward_value, Terrain terrain_info);

	private:
		/** @brief Threshold that specify the flat condition */
		double flat_threshold_;

		/** @brief Threshold that indicates a very (bad) steep condition */
		double steep_threshold_;
};


} //@namespace environment
} //@namespace dwl

#endif
