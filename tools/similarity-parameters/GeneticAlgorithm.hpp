/*
 * Copyright (C) 2019 Emeric Poupon
 *
 * This file is part of LMS.
 *
 * LMS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LMS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LMS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <numeric>

#include "utils/Utils.hpp"
#include "ParallelFor.hpp"

template<typename Individual>
class GeneticAlgorithm
{
	public:
		using Score = float;

		using BreedFunction = std::function<Individual(const Individual&, const Individual&)>;
		using MutateFunction = std::function<void(Individual&)>;
		using ScoreFunction = std::function<Score(const Individual&)>;

		struct Params
		{
			std::size_t		nbWorkers {1};
			std::size_t		nbGenerations;
			float			mutationProbability {0.05};
			BreedFunction	breedFunction;
			MutateFunction		mutateFunction;
			ScoreFunction		scoreFunction;
		};

		GeneticAlgorithm(const Params& params);

		// Returns the individual that has the maximum score after processing the requested generations
		Individual simulate(const std::vector<Individual>& initialPopulation);

	private:

		struct ScoredIndividual
		{
			Individual individual;
			std::optional<Score> score {};
		};

		void scoreAndSortPopulation(std::vector<ScoredIndividual>& population);

		Params _params;
};

template<typename Individual>
GeneticAlgorithm<Individual>::GeneticAlgorithm(const Params& params)
: _params {params}
{
}

template<typename Individual>
Individual
GeneticAlgorithm<Individual>::simulate(const std::vector<Individual>& initialPopulation)
{
	if (initialPopulation.size() < 10)
		throw std::runtime_error("Initial population must has at least 10 elements");

	std::vector<ScoredIndividual> scoredPopulation;
	scoredPopulation.reserve(initialPopulation.size());

	std::transform(std::cbegin(initialPopulation), std::cend(initialPopulation), std::back_inserter(scoredPopulation ),
			[](const Individual& individual) { return ScoredIndividual {individual};});

	scoreAndSortPopulation(scoredPopulation);

	for (std::size_t currentGeneration {}; currentGeneration  < _params.nbGenerations; ++currentGeneration)
	{
		std::cout << "Processing generation " << currentGeneration << "..." << std::endl;
		// parent selection (elitist selection)
		scoredPopulation.resize(scoredPopulation.size() / 2);

		// breed the remaining individuals
		std::vector<ScoredIndividual> children;
		children.reserve(initialPopulation.size() - scoredPopulation.size());

		while (children.size() + scoredPopulation.size() < initialPopulation.size())
		{
			// Select two random parents
			const auto itParent1 {pickRandom(scoredPopulation)};
			const auto itParent2 {pickRandom(scoredPopulation)};

			if (itParent1 == itParent2)
				continue;

			ScoredIndividual child {_params.breedFunction(itParent1->individual, itParent2->individual)};
			
			if (getRandom(0, 100) <= _params.mutationProbability * 100)
				_params.mutateFunction(child.individual);

			children.emplace_back(std::move(child ));
		}

		scoredPopulation.insert(std::end(scoredPopulation), std::make_move_iterator(std::begin(children)), std::make_move_iterator(std::end(children)));
		assert(scoredPopulation.size() == initialPopulation.size());

		scoreAndSortPopulation(scoredPopulation);

		std::cout << "Current best score = " << *scoredPopulation.front().score << std::endl;
	}

	std::cout << "Best score = " << *scoredPopulation.front().score << std::endl;
	return scoredPopulation.front().individual;
}


template<typename Individual>
void
GeneticAlgorithm<Individual>::scoreAndSortPopulation(std::vector<ScoredIndividual>& scoredPopulation)
{
	parallel_foreach(_params.nbWorkers, std::begin(scoredPopulation), std::end(scoredPopulation),
			[&](ScoredIndividual& scoredIndividual)
			{
				if (!scoredIndividual.score)
					scoredIndividual.score = _params.scoreFunction(scoredIndividual.individual);
			});

	std::sort(std::begin(scoredPopulation), std::end(scoredPopulation), [](const ScoredIndividual& a, const ScoredIndividual& b) { return a.score > b.score; });
}
