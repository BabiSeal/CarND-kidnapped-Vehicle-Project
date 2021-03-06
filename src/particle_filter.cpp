/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 *      Modified: Babi Seal
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <assert.h>
#include "particle_filter.h"

using namespace std;
const double pi = M_PI; // Constant PI


/**
 * init Initializes particle filter by initializing particles to Gaussian
 *   distribution around first position and all the weights to 1.
 * @param x Initial x position [m] (simulated estimate from GPS)
 * @param y Initial y position [m]
 * @param theta Initial orientation [rad]
 * @param std[] Array of dimension 3 [standard deviation of x [m], standard deviation of y [m]
 *   standard deviation of yaw [rad]]
 */
void ParticleFilter::init(double x, double y, double theta, double std[]) {

  default_random_engine gen;

  // Set the number of particles to 75
  num_particles = 75;
  particles.resize(num_particles);
  weights.resize(num_particles);

  // Initialize all particles to first position (based on estimates of 
  // x, y, theta and their uncertainties from GPS) and all weights to 1. 
  // Add random Gaussian noise to each particle.
  double std_x, std_y, std_theta;
  std_x = std[0];
  std_y = std[1];
  std_theta = std[2];

  // Create a normal (Gaussian) distribution for x, y, theta.
  normal_distribution<double>dist_x(x, std_x);
  normal_distribution<double>dist_y(y, std_y);
  normal_distribution<double>dist_theta(theta, std_theta);
  for (int i=0; i < num_particles; i++)
    {
      particles[i].id = i;
      particles[i].x = dist_x(gen);
      particles[i].y = dist_y(gen);
      particles[i].theta = dist_theta(gen);
      particles[i].weight = 1;
      weights[i] = 1;
    }
  is_initialized = true;
}

/**
 * prediction Predicts the state for the next time step
 *   using the process model.
 * @param delta_t Time between time step t and t+1 in measurements [s]
 * @param std_pos[] Array of dimension 3 [standard deviation of x [m], standard deviation of y [m]
 *   standard deviation of yaw [rad]]
 * @param velocity Velocity of car from t to t+1 [m/s]
 * @param yaw_rate Yaw rate of car from t to t+1 [rad/s]
 */
void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
  
  default_random_engine gen;
  double std_x, std_y, std_yaw;

  std_x = std_pos[0];
  std_y = std_pos[1];
  std_yaw = std_pos[2];

  normal_distribution<double> dist_x(0, std_x);
  normal_distribution<double>dist_y(0, std_y);
  normal_distribution<double>dist_yaw(0, std_yaw);
  double theta_0;
  double particle_x;
  double particle_y;
  double particle_theta;
  double noise_x;
  double noise_y;
  double noise_theta;

  for (int i=0; i < num_particles; i++)
    {
      theta_0 = particles[i].theta;
      // take into account potential for zero yaw-rate
      if (fabs(yaw_rate) > 0.0) {
        particle_x = particles[i].x + (velocity/yaw_rate)*(sin(theta_0 + yaw_rate*delta_t) - sin(theta_0));
        particle_y = particles[i].y + (velocity/yaw_rate)*(cos(theta_0) - cos(theta_0 + yaw_rate*delta_t));
        particle_theta = particles[i].theta + yaw_rate * delta_t;
      } else {
        particle_x = particles[i].x + (velocity*delta_t*cos(theta_0));
        particle_y = particles[i].y + (velocity*delta_t*sin(theta_0));
        particle_theta = particles[i].theta;
      }
      noise_x = dist_x(gen);
      noise_y = dist_y(gen);
      noise_theta = dist_yaw(gen);

      particles[i].x = particle_x + noise_x;
      particles[i].y = particle_y + noise_y;
      particles[i].theta = particle_theta + noise_theta;
      particles[i].weight = 1;
      weights[i] = 1;
    }
}

/**
 * GetLandMarksWithinSensorRange
 * Find all landmarks that are within the senor-range from the particle
 * 
 * @param curr_p Pointer to current particle
 * @param map_landmarks Vector of all landmarks on map
 * @param sensor_range  Senor Range of LIDAR
 * @param remaining_landmarks (OUT) return by reference all landmarks that lie within sensor_range
 */
void GetLandmarksWithinSensorRange(Particle *curr_p, 
                                   std::vector<Map::single_landmark_s> map_landmarks, 
                                   double sensor_range, 
                                   std::vector<LandmarkObs> &remaining_landmarks)
{
  for (int i = 0; i < map_landmarks.size(); i++)
    {
      double curr_dist = dist(curr_p->x, curr_p->y,
                              (double)map_landmarks[i].x_f,
                              (double)map_landmarks[i].y_f);
      LandmarkObs lmk;
      if (curr_dist <= sensor_range) {
        lmk.id = map_landmarks[i].id_i;
        lmk.x = (double) map_landmarks[i].x_f;
        lmk.y = (double) map_landmarks[i].y_f;
        remaining_landmarks.push_back(lmk);
       }
    }
}

/**
 * ConvertToParticleCoordinates
 * Convert car observations to global map coordinates using rotation and translation formulas
 * 
 * @param curr_p Pointer to current particle
 * @param observations Vector of car observations
 * @param transformed_observations (OUT) predicted measurements of observed landmarks in global map coordinates
 */
void ConvertToParticleCoordinates(Particle *current_particle, 
                                  std::vector<LandmarkObs>& observations,
                                  std::vector<LandmarkObs>& transformed_observations)
{
  transformed_observations.resize(observations.size());
  double xt = current_particle->x;
  double yt = current_particle->y;
  double theta = current_particle->theta;
  double x;
  double y;
  for (int i = 0; i < observations.size(); i++)
    {
      x = observations[i].x;
      y = observations[i].y;
      transformed_observations[i].id = observations[i].id;
      transformed_observations[i].x = xt + x*cos(theta) - y*sin(theta);
      transformed_observations[i].y = yt + x*sin(theta) + y*cos(theta);
    }
}


/**
 * UpdateParticleWeight
 * Update the weight of the particle by taking the product of the Bivariate Gaussian Distributions of
 * each observation.
 * 
 * @param curr_p Pointer to current particle
 * @param particle_index index of the particle to update weights
 * @param std_landmark sigmax and sigmay
 * @param observations Measured x,y
 * @param transformed_observations Mean (mux, muy) of the bivariate gaussian distributions
 */
void UpdateParticleWeight(Particle *curr_p,
                          int particle_index,
                          double std_landmark[],
                          std::vector<LandmarkObs> &observations, 
                          std::vector<LandmarkObs> &transformed_observations)
{
  double updated_weight = 1.0;
  double temp_weight = 1.0;
  double sigmax = std_landmark[0];
  double sigmay = std_landmark[1];
  double l1 = (1/ (2 * pi * sigmax * sigmay));

  for (int i = 0 ; i < observations.size(); i++)
    {
      double x = observations[i].x;
      double y = observations[i].y;
      double mux = transformed_observations[i].x;
      double muy = transformed_observations[i].y;

      double l2 = ((x - mux)*(x - mux))/(2*sigmax*sigmax);
      double l3 = ((y - muy)*(y - muy))/(2*sigmay*sigmay);
      double l4 = exp(- (l2 + l3));
      
      temp_weight = l1 * l4;
      updated_weight = updated_weight*temp_weight;
    }
  curr_p->weight = updated_weight;
}

/**
 * dataAssociation Finds which observations correspond to which landmarks (likely by using
 *   a nearest-neighbors data association).
 * @param predicted Vector of predicted landmark observations
 * @param observations Vector of landmark observations
 */
void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {

  // Find the predicted landmark measurement that is closest to each observed measurement and assign the 
  // observed measurement to this particular landmark.
  for (int i = 0; i < observations.size(); i++)
    {
      double distance_so_far = 100000.0; int closest = 0;
      for (int j = 0; j < predicted.size(); j++)
        {
          double curr_dist = dist(observations[i].x, observations[i].y, predicted[j].x, predicted[j].y);
          if (curr_dist < distance_so_far) {
            distance_so_far = curr_dist;
            closest = j;
          }
        }
      observations[i].id = closest;
      observations[i].x = predicted[closest].x;
      observations[i].y = predicted[closest].y;
    }
}

/**
 * updateWeights Updates the weights for each particle based on the likelihood of the 
 *   observed measurements. 
 * @param sensor_range Range [m] of sensor
 * @param std_landmark[] Array of dimension 2 [standard deviation of range [m],
 *   standard deviation of bearing [rad]]
 * @param observations Vector of landmark observations
 * @param map Map class containing map landmarks
 */
void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		std::vector<LandmarkObs> observations, Map map_landmarks) {
  
  for (int i = 0 ; i < num_particles; i++ )
    {
      Particle *curr_p = &particles[i];
      std::vector<LandmarkObs> remaining_landmarks;
      std::vector<LandmarkObs> chosen_matched_observations;
      std::vector<LandmarkObs> converted_observations;
      
      //1. Consider only landmarks that will be within sensor range of the particle
      remaining_landmarks.resize(0);
      GetLandmarksWithinSensorRange(curr_p, map_landmarks.landmark_list, sensor_range, remaining_landmarks);

      //2. Convert the car observations into global map coordinates
      ConvertToParticleCoordinates(curr_p, observations, converted_observations);

      chosen_matched_observations = converted_observations;

      //3. Do a nearest-match to associate each tranformed observation to the closest landmark 
      dataAssociation(remaining_landmarks, chosen_matched_observations);

      //4. Update the particle weight by computing the product of the bivariate-random distributions
      //   of the observation, from the predicted landmark position. Higher weight means this particle
      //   is more likely the position of the car.
      UpdateParticleWeight(curr_p, i, std_landmark, converted_observations, chosen_matched_observations);
      weights[i] = curr_p->weight;
    }
}

/**
 * resample Resamples from the updated set of particles to form
 *   the new set of particles.
 */
void ParticleFilter::resample() {
  // Resample particles with replacement with probability proportional to their weight. 
  int index;
  std::random_device rd;
  std::mt19937 gen(rd()); 
  //default_random_engine gen;
  discrete_distribution<> dist_N(weights.begin(), weights.end());

  std::vector<Particle> particles_temp;
  std::vector<double> weights_temp;
  particles_temp.resize(num_particles);
  weights_temp.resize(num_particles);
  
  for (int i = 0; i < num_particles; ++i)
    {
      int index = dist_N(gen);
      weights_temp[i] = particles[index].weight;
      particles_temp[i] = particles[index];
    }
  weights = weights_temp;
  particles = particles_temp;
}

void ParticleFilter::write(std::string filename) {
	// You don't need to modify this file.
	std::ofstream dataFile;
	dataFile.open(filename, std::ios::app);
	for (int i = 0; i < num_particles; ++i) {
		dataFile << particles[i].x << " " << particles[i].y << " " << particles[i].theta << "\n";
	}
	dataFile.close();
}
