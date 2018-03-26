#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"
#include "chkmimic.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

Cfg mk_init_cfg(const Program& prog, const Observer& linobs) {
	std::size_t numThreads = 1;

	Cfg init(
		{{ &prog.init(), NULL, NULL }},
		linobs.initial_state(),
		new Shape(linobs.numVars(), prog.numGlobals(), prog.numLocals(), numThreads)
	);
	while (init.pc[0] != NULL) {
		std::vector<Cfg> postpc = tmr::post(init, 0);
		init = std::move(postpc.front());
	}
	return init;
}


/******************************** WORK SET ********************************/

RemainingWork::RemainingWork(Encoding& enc) : _enc(enc) {}

const Cfg& RemainingWork::pop() {
	const Cfg* top = *_work.begin();
	_work.erase(_work.begin());
	return *top;
}

static bool output = false;

void RemainingWork::add(Cfg&& cfg) {
	// std::cout << "Adding: " << cfg << *cfg.shape << std::endl;
	// if (cfg.shape->test(5,1,EQ)) { std::cout << std::endl << "5=1" << std::endl; exit(0); }
	// if (cfg.shape->test(6,1,EQ) && cfg.freed) { std::cout << std::endl << "6=1 freed" << std::endl; exit(0); }
	// if (cfg.shape->test(1,3,EQ)) { std::cout << std::endl << "1=3" << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.guard0state.at(7) && !cfg.guard0state.at(7)->is_initial() && !cfg.valid_ptr.at(7)) { std::cout << std::endl << "invalid guard" << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()==30 && !cfg.valid_ptr.at(6) && cfg.shape->test(5, 7, EQ)) { std::cout << "node invalid with option to CAS" << std::endl; exit(0); }
	// if (cfg.own.at(6) && haveCommon(cfg.shape->at(5,6), EQ_MT_GT)) { std::cout << "owned node shared reachable" << std::endl; exit(0); }
	// if (cfg.state.states().at(4)->name() == "u5" && cfg.shape->test(5,0,EQ)) { std::cout << "foobar: u5 with option of empty stack" << std::endl; /*exit(0);*/ }
	// if (cfg.pc[0] && cfg.pc[0]->id()>=29 && cfg.pc[0]->id()<=35 && !cfg.guard0state.at(7)) { std::cout << "top has no smr state" << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()>20 && cfg.pc[0]->id()<=27 && cfg.guard0state.at(7)->name() == "r") { std::cout << "top has 'r' smr state" << std::endl << cfg << *cfg.shape << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()>=22 && cfg.pc[0]->id()<=27 && cfg.guard0state.at(7)->name() == "r" && cfg.shape->test(5,7,EQ)) { std::cout << "top has 'r' smr state despite being shared reachable" << std::endl << cfg << *cfg.shape << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()==27 && cfg.guard0state.at(7)->name() == "rg" && cfg.shape->test(5,7,EQ)) { std::cout << "top has 'rg' smr state despite being shared reachable" << std::endl << cfg << *cfg.shape << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()>=29 && cfg.pc[0]->id()<=35 && cfg.guard0state.at(7)->name() == "rg") { std::cout << "top has 'rg' smr state" << std::endl << cfg << *cfg.shape << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()==27 && cfg.guard0state.at(7)->name() == "rg" && cfg.shape->test(5,7,EQ)) { std::cout << "top has 'rg' smr state despite being shared reachable" << std::endl << cfg << *cfg.shape << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()>=29 && cfg.pc[0]->id()<=30 && cfg.guard0state.at(6) && cfg.guard0state.at(6)->is_special() && cfg.shape->test(5,7, EQ)) { std::cout << "node retired despite top being guarded and shared reachable" << std::endl; exit(0); }
	// if (cfg.pc[0] && cfg.pc[0]->id()==30 && cfg.shape->at(7,6) != MT_) { std::cout << "node not next of top despite guard" << std::endl; exit(0); }
	// if (cfg.shape->test(7,1, EQ) && cfg.freed && cfg.valid_ptr.at(7)) { std::cout << "top valid despite being potentially freed" << std::endl; exit(0); }
	// if (output && cfg.pc[0] && cfg.pc[0]->id()==26) { std::cout << "Adding after free: " << cfg << *cfg.shape << std::endl; exit(0); }


	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& linobs) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);

	// add all seen possibilities -- this saves interference
	Cfg init00 = mk_init_cfg(prog, linobs);
	Cfg init01 = init00.copy();
	Cfg init10 = init00.copy();
	Cfg init11 = init00.copy();
	init01.seen[0] = true;
	init10.seen[1] = true;
	init11.seen[0] = true;
	init11.seen[1] = true;
	work.add(std::move(init00));
	work.add(std::move(init01));
	work.add(std::move(init10));
	work.add(std::move(init11));

	// fixpoint
	while (!work.done()) {
		std::size_t counter = 0;

		std::cerr << "post image...     ";
		// std::cout << std::endl << std::endl << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		while (!work.done()) {
			const Cfg& topost = work.pop();
			SEQUENTIAL_STEPS++;

			// output = topost.pc[0] && topost.pc[0]->id()==26 && topost.shape->test(7,1,EQ) && topost.shape->test(7,5,MT) && topost.guard0state.at(7) && topost.guard0state.at(7)->is_special();
			if (output) {
				std::cout << "===============================" << std::endl;
				std::cout << "Post for: " << topost << *topost.shape << std::endl << "-------------------------------" << std::endl;
			}
			work.add(tmr::mk_all_post(topost, prog));
			output = false;
			
			counter++;
			if (counter%10000 == 0) std::cerr << "[" << counter/1000 << "k-" << enc->size()/1000 << "k]";
		}
		std::cerr << " done! [enc.size()=" << enc->size() << ", iterations=" << counter << ", enc.bucket_count()=" << enc->bucket_count() << "]" << std::endl;
		output = false;

		tmr::mk_all_interference(*enc, work);
	}

	std::cout << std::endl << "Fixed point computed " << enc->size() << " distinct configurations." << std::endl;
	return enc;
}
