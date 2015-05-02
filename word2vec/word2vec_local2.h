/*
 * word2vec with local parameter cache
 *
 * parameter cache cantains parameters in a mini-batch
 */
#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "swiftsnails.h"
#include "./param.h"
#include "./access_method.h"

#define MAX_STRING 100
#define EXP_TABLE_SIZE 1000
#define MAX_EXP 6
#define MAX_SENTENCE_LENGTH 1000
#define MAX_CODE_LENGTH 40
using namespace std;
using namespace swift_snails;
typedef float real_t;                    // Precision of float numbers
const int table_size = 1e8;
using namespace std;

// total number of words in dataset
size_t train_words = 0;
long long file_size = 0;
long long word_count_actual = 0;
clock_t start;

struct Random {
    Random(unsigned long long seed) {
        next_random = seed;
    }

    unsigned long long operator() () {
        next_random = next_random * (unsigned long long) 25214903917 + 11;
        return next_random;
    }

private:
    unsigned long long next_random  = 0;
};


void ReadWord(char *word, FILE *fin) {
	int a = 0, ch;
	while (!feof(fin)) {
		ch = fgetc(fin);
		if (ch == 13)
			continue;
		if ((ch == ' ') || (ch == '\t') || (ch == '\n')) {
			if (a > 0) {
				if (ch == '\n')
					ungetc(ch, fin);
				break;
			}
			if (ch == '\n') {
				strcpy(word, (char * )"</s>");
				return;
			} else
				continue;
		}
		word[a] = ch;
		a++;
		if (a >= MAX_STRING - 1)
			a--;   // Truncate too long words
	}
	word[a] = 0;
}

long ReadWordKey(FILE *file) {
    char word[MAX_STRING];
    ReadWord(word, file);
    if (feof(file)) return -1;
    char *end;
    if (strcmp(word, "</s>") == 0) return -2;
    long key  = strtol(word, &end, 10);
    return key;
}


struct Error {
    real_t data = 0;
    size_t counter = 0;

    void accu(real_t e) {
        data += e;
        counter ++;
    }
    
    real_t norm() {
        real_t error = data / counter;
        data = 0;
        counter = 0;
        return error;
    }
};

struct Vocab {
	typedef size_t id_t;
    typedef index_t key_t;
	struct WordItem {
		int count = 0;
		id_t id = std::numeric_limits<id_t>::max();
	};  // end struct WordItem

	std::pair<key_t, WordItem>* worditems = NULL;
	map<key_t, WordItem> vocab;
    int *table = NULL;


	Vocab() {
		//vocab.set_empty_key(std::numeric_limits<id_t>::max());
		len_vec = global_config().get_config("len_vec").to_int32();
		min_reduce = global_config().get_config("min_reduce").to_int32();
		min_len = global_config().get_config("min_len").to_int32();
		CHECK_GT(len_vec, 0);
		CHECK_GT(min_reduce, 0);
		CHECK_GT(min_len, 0);
	}

    ~Vocab() {
        if(worditems != NULL) {
            delete worditems;
            worditems = NULL;
        }

        if(table != NULL) {
            delete table;
            table = NULL;
        }
    }

    void init() {
        init_worditems();
        init_unigram_table();
    }

	void inc_word(key_t key) {
		if (vocab.count(key) == 0) {
			vocab[key] = WordItem();
			vocab[key].id = word_counter++;
		}
		vocab[key].count++;
	}
    // set vocab to initial status
    void clear() {
        vocab.clear();
        // delete worditems
        CHECK(worditems != NULL);
        delete worditems;
        worditems = NULL;
        worditems_size = 0;
        word_counter = 0;
    }
    // init vocab from file
    void operator() (const string& path) 
    {
        init(path);
        //init_worditems();
        //init_unigram_table();
    }

    WordItem& operator[] (key_t key) {
        return vocab[key];
    }

    id_t word2id(key_t key) {
        return vocab[key].id;
    }

    key_t id2word(id_t id) {
        return worditems[ table[id] ].first;
    }

	size_t size() const {
		return vocab.size();
	}
protected:
	// init from a file
	void init(const std::string& path) {
		FILE *file = fopen(path.c_str(), "rb");
		if (file == NULL) {
			fprintf(stderr, "no such file or directory: %s", path.c_str());
			exit(1);
		}
        long word = 0;
        while(true) {
            word = ReadWordKey(file);
            if(feof(file)) break;
            if(word == -1 || word == -2) continue;
            inc_word(word);
            train_words++;
            if (train_words % 100000 == 0) {
                printf("%lldK%c", train_words / 1000, 13);
                fflush(stdout);
            }
        }
        LOG(INFO) << "file_size:\t" << file_size;
		file_size = ftell(file);
        DLOG(INFO) << "to create worditems";
		//worditems.reset(new std::pair<key_t, WordItem>[vocab.size()]);
		LOG(INFO)<< "vocabulary load\t" << vocab.size() << "\t words";
		LOG(INFO)<< "dataset has\t" << train_words << "\twords";
		fclose(file);
	}

    void init_worditems() {
        RAW_DLOG(INFO, "... init_worditems");
        CHECK(!vocab.empty());
        CHECK(worditems == NULL);
        reset_worditems(vocab.size());
		for (const auto& item : vocab) {
			worditems[item.second.id] = item;
		}
    }

	void init_unigram_table() {
		RAW_DLOG(INFO, "... init_unigram_table");
		CHECK_GT(vocab.size(), 0) << "word_freq should be inited before";
		int a, i;
		double train_words_pow = 0;
		double d1, power = 0.75;
		//table.reset(new int[table_size]);
        if(table == NULL) table = new int[table_size];

		i = 0;
		for(const auto& item : vocab) {
			train_words_pow += std::pow(item.second.count, power);
		}
		i = 0;
		d1 = pow(worditems[i].second.count, power) / (double)train_words_pow;
		for(a = 0; a < table_size; a++) {
			table[a] = worditems[i].first;
            //CHECK(table[a] != 19987);
			if(a / (double)table_size > d1) {
				i++;
				d1 += pow(worditems[i].second.count, power) / (double)train_words_pow;
			}
			if(i >= vocab.size()) i = vocab.size() - 1;
		}
	}

    void reset_worditems(size_t size) {
        CHECK_GT(size, 0);
        if(worditems_size == size) return;
        if(worditems != NULL) {
            delete worditems;
            worditems_size = 0;
        }
        worditems = new std::pair<key_t, WordItem>[size];
        worditems_size = size;
    }

	// clean the unusual words
	void clean_vocab() {
		if (min_reduce > 0) {
			for (auto it = vocab.begin();; it != vocab.end()) {
				auto& item = it->second;
				if (item.count < min_reduce)
					vocab.erase(it++);
			}
		}
	}

	//std::unique_ptr<real[]> feas;
	size_t word_counter = 0;
	int len_vec = 0;
	int min_reduce = 0;
	int min_len = 0;
    size_t worditems_size = 0;
	const id_t maxid = std::numeric_limits<id_t>::max();
}; // end struct Vocab



class LocalCache {
public:
    typedef index_t          key_t;
    typedef Word2VecParam val_t;
    typedef Word2VecGrad grad_t;
    using param_cache_t = GlobalParamCache<key_t, val_t, grad_t>;
    using pull_access_t = GlobalPullAccess<key_t, val_t, grad_t>;
    using push_access_t = GlobalPushAccess<key_t, val_t, grad_t>;

    LocalCache(Vocab &local_vocab) :
        pull_access(global_pull_access<key_t, val_t, grad_t>()),
        push_access(global_push_access<key_t, val_t, grad_t>()), 
        local_vocab(local_vocab)
    { }

    // collect keys before pull
    // init local cache and pull
    void pull() {
        // init local_vocab from collected keys
        local_vocab.init();
        init_local_cache();
        DLOG(INFO) << "to pull from PS";
        CHECK(! local_keys.empty());
        DLOG(INFO) << "pull " << local_keys.size() << " keys";
        pull_access.pull_with_barrier(local_keys, param_cache);
        DLOG(INFO) << "get pulled param " << param_cache.size();
    }
    // push and clear local cache
    void push() {
        DLOG(INFO) << "to push to PS, key count:\t" << local_keys.size();
        //int i = 0;
        //for(auto& key : local_keys) {
            //if(i ++ > 10) break;
            //LOG(INFO) <<  "grad for " << key << " " << param_cache.grads()[key];
        //}
        if(local_keys.empty()) {
            RAW_DLOG(INFO, "local_keys empty, do not push");
            return;
        }
        push_access.push_with_barrier(local_keys, param_cache);
        DLOG(INFO) << "end push to PS";
    }

    void clear() {
        DLOG(INFO) << "to clear local_param";
        local_keys.clear();
        param_cache.clear();
    }

    param_cache_t& cache() {
        return param_cache;
    }

protected:
    void init_local_cache() {
        DLOG(INFO) << "... init local cache";
        CHECK(local_keys.empty());
        CHECK(!local_vocab.vocab.empty());
        DLOG(INFO) << "> to init local_vocab";
        for(auto& item : local_vocab.vocab) {
            local_keys.insert(item.first);
        }
        DLOG(INFO) << "> to init param_cache";
        // init param cahce
        for(auto &key : local_keys) {
            param_cache.init_key(key);
        }
    }

    Vocab& local_vocab;
    GlobalParamCache<key_t, val_t, grad_t> param_cache;
    std::unordered_set<key_t> local_keys;
    pull_access_t &pull_access;
    push_access_t &push_access;
};


class MiniBatch {
public:
    typedef index_t          key_t;
    typedef Word2VecParam val_t;
    typedef Word2VecGrad grad_t;
    size_t num_sents_to_cache = 0;
    int num_sents_each_thread = 0;

    Vocab local_vocab;
    Vocab &global_vocab;
    LocalCache local_cache;

    MiniBatch(Vocab &global_vocab) :
        global_vocab(global_vocab), 
        local_cache(local_vocab)
    { 
        num_sents_to_cache = global_config().get_config("num_sents_to_cache").to_int32();
        num_threads = global_config().get_config("async_channel_thread_num").to_int32();
        num_sents_each_thread = num_sents_to_cache / num_threads;
        CHECK_GT(num_sents_to_cache, 0);
        CHECK_GT(num_sents_each_thread, 0);
    }

    // collect words from following sententences
    size_t collect_keys(FILE *file) {
        RAW_DLOG(INFO, "minibatch to collect keys");
        //long cur_pos = ftell(file);
        size_t word_counter = 0;
        char word[MAX_STRING];
        size_t sentence_counter = 0;
        long file_pos;
        size_t key;
        char *end;
        // read words
        for(;;) {
            if (feof(file)) break;
            if(sentence_counter >= num_sents_to_cache) break;
            key = ReadWordKey(file);
            if(key == -1) break;
            if(key == -2) {
                if(sentence_counter++ % num_sents_each_thread == 0) {
                    if(_file_poses.size() < num_threads) {
                        file_pos = ftell(file);
                        //LOG(INFO) << "push file_pos:\t" << file_pos;
                        _file_poses.push_back(file_pos);
                    }
                }
                continue;
            }
            //word_count ++;
            if(global_vocab.vocab.count(key) > 0) local_vocab.inc_word(key);
        }
        // resume position
        //fseek(file, cur_pos, SEEK_SET);
        RAW_DLOG(INFO, "minibatch collect %d keys", local_vocab.size());
        return local_vocab.size();
    }

    LocalCache::param_cache_t& cache() {
        return local_cache.cache();
    }

    void pull() {
        //local_cache.init(); // parameter cache init from local_vocab
        local_cache.pull();
    }

    void push() {
        local_cache.push();
        DLOG(INFO) << "> to clear local_vocab";
        local_vocab.clear();
        DLOG(INFO) << "> to clear local cache";
        local_cache.clear();
        DLOG(INFO) << "> to clear file poses";
        _file_poses.clear();
    }

    const vector<long>& file_poses() const {
        return _file_poses;
    }

private:
    int num_threads = 0;
    // task for each thread
    std::vector<long> _file_poses;
};


class Word2Vec {

public:
    typedef index_t          key_t;
    typedef Word2VecParam val_t;
    typedef Word2VecGrad grad_t;

	Word2Vec(const string& data_path) :
        data_path(data_path)
    {
		min_len = global_config().get_config("min_len").to_int32();
		len_vec = global_config().get_config("len_vec").to_int32();
		num_threads =
		        global_config().get_config("async_channel_thread_num").to_int32();
		window = global_config().get_config("window").to_int32();
		negative = global_config().get_config("negative").to_int32();
		cbow = global_config().get_config("cbow").to_int32() > 0;
        num_sents_to_cache = global_config().get_config("num_sents_to_cache").to_int32();
		CHECK_GT(min_len, 0);
		CHECK_GT(num_threads, 0);
	}

    ~Word2Vec() {
        free(expTable);
    }

    void operator() () {
        LOG(WARNING) << "... init global vocab";
        global_vocab(data_path);
        //init_net();
        init_exp_table();
        //init_unigram_table();
        //global_param.init();
    }

    void train() {
        Error global_error;
        MiniBatch minibatch(global_vocab);
        int num_iters = global_config().get_config("num_iters").to_int32();
        LOG(WARNING) << "start " << num_threads << " to train " << data_path;

        for(int iter = 0; iter < num_iters; iter++) {
            train_iter(data_path, global_error, num_threads);
            LOG(WARNING) << iter << " th train!";
            RAW_LOG(WARNING, "error:\t%f", global_error.norm());
            word_count_actual = 0;
        }
    }

    void train_iter(const string &train_file, Error& global_error, int thread_num) {
        MiniBatch minibatch(global_vocab);
		FILE *fi = fopen(train_file.c_str(), "rb");
        AsynExec as(thread_num); auto channel = as.open();

        while(true) {
            // TODO fix statebarrier block state
            StateBarrier state;
            // collect keys from num sents
            size_t size = minibatch.collect_keys(fi);
            if(size < 10) break;
            minibatch.pull();
            CHECK_LE(minibatch.file_poses().size(), thread_num);
            std::atomic<size_t> finish_counter{minibatch.file_poses().size()};
            int id = 0; 
            for(auto file_pos : minibatch.file_poses()) {
                auto thread_task = [this, id, &train_file, file_pos, &global_error, &minibatch, &finish_counter, &state] {
                    TrainModelThread(id, train_file, file_pos, global_error, minibatch);
                    if( -- finish_counter == 0) {
                        state.set_state_valid();
                        state.try_unblock();
                    }
                };
                channel->push(std::move(thread_task));
                id ++;
            }
            state.block();
            // all worker threads have finished work
            minibatch.push();
            if (feof(fi)) break;
        }
    }

	void TrainModelThread(int id, const string &train_file, long file_pos, Error& global_error, MiniBatch &minibatch) {
        //LocalParamCache local_param;
        Random random((unsigned long long)id);
		long long a, b, d, word, last_word, sentence_length = 0, sentence_position = 0;
		long long word_count = 0, last_word_count = 0, sen[MAX_SENTENCE_LENGTH + 1];
		long long l1, l2, c, target, label;
        long sentence_counter = 0;
        long last_sentence_counter = -1;
		//unsigned long long next_random = (long long) id;
		real_t f, g;
		clock_t now;
        auto& params = minibatch.cache().params();
        auto& grads = minibatch.cache().grads();
        auto& vocab = minibatch.local_vocab;
        Vec neu1(len_vec);
        Vec neu1e(len_vec);
		FILE *fi = fopen(train_file.c_str(), "rb");
		if (fi == NULL) {
			fprintf(stderr, "no such file or directory: %s", train_file.c_str());
			exit(1);
		}
		fseek(fi, file_pos, SEEK_SET);
		while (true) {
			if (word_count - last_word_count > 10000000) {
				word_count_actual += word_count - last_word_count;
				last_word_count = word_count;
				if ((debug_mode)) {
					now = clock();
					printf(
					        "%cAlpha: %f  Progress: %.2f%%  Words/thread/sec: %.2fk  ",
					        13, alpha,
					        word_count_actual / (real_t) (train_words + 1) * 100,
					        word_count_actual
					                / ((real_t) (now - start + 1)
					                        / (real_t) CLOCKS_PER_SEC * 1000));
					fflush(stdout);
				}
				alpha = starting_alpha
				        * (1 - word_count_actual / (real_t) (train_words + 1));
                // TODO alpha should be inited again in the second iteration
			}
            // read a sentence
			if (sentence_length == 0) {
				while (1) {
					word = ReadWordKey(fi);
					if (feof(fi))
						break;
					if (word == -1)
						continue;
					word_count++;
					if (word == -2) // get </s> or \n
                    {
                        if(sentence_counter ++ > minibatch.num_sents_each_thread) break;
                    } 
                    
					// The subsampling randomly discards frequent words while keeping the ranking same
					if (sample > 0) {
						real_t ran = (sqrt(vocab[word].count / (sample * train_words))
						        + 1) * (sample * train_words) / vocab[word].count;
						//next_random = next_random * (unsigned long long) 25214903917
						        //+ 11;
						if (ran < (random() & 0xFFFF) / (real_t) 65536)
							continue;
					}

                    // skip broken words
                    if(vocab.vocab.count(word) == 0) continue;

					sen[sentence_length] = word;
					sentence_length++;
					if (sentence_length >= MAX_SENTENCE_LENGTH)
						break;
				}
				sentence_position = 0;
			}
			if (feof(fi)) break;
			word = sen[sentence_position];
			if (word == -1) continue;
			for (c = 0; c < len_vec; c++) neu1[c] = 0;
			for (c = 0; c < len_vec; c++) neu1e[c] = 0;
			b = random() % window;
			if (cbow) {  //train the cbow architecture
				// in -> hidden
				for (a = b; a < window * 2 + 1 - b; a++)
					if (a != window) {
						c = sentence_position - window + a;
						if (c < 0)
							continue;
						if (c >= sentence_length)
							continue;
						last_word = sen[c];
						if (last_word == -1)
							continue;
                        if(vocab.vocab.count(last_word) == 0) {
                            DLOG(INFO) << "vocab not found word:\t" << last_word;
                            continue;
                        }
                        if(params.count(last_word) == 0) continue;
                        //if(grads.count(last_word) == 0) continue;
                        Vec &syn0_lastword = params[last_word].v();
                        //LOG(INFO) << "syn0_lastword:\t" << syn0_lastword;
                        //}
						for (c = 0; c < len_vec; c++) {
                            neu1[c] += syn0_lastword[c];
                        }
                        //LOG(INFO) << "cal neu1:\t" << neu1;
					}

				// NEGATIVE SAMPLING
				if (negative > 0)
					for (d = 0; d < negative + 1; d++) {
						if (d == 0) {
							target = word;
							label = 1;
						} else {
							//next_random = next_random
							        //* (unsigned long long) 25214903917 + 11;
							target = vocab.table[(random() >> 16) % table_size];
							if (target == 0)
                                target = vocab.table[(random() >> 16) % table_size];
								//target = random() % (vocab.size() - 1) + 1;
							if (target == word)
								continue;
                            if (target == -1 || target == -2) continue;

							label = 0;
						}

                        if(target == -1 || target == -2) continue;

                        //if (vocab.vocab.count(target) == 0) continue;
                        //if (params.count(target) == 0) continue;
                        if (grads.count(target) == 0) continue;
                        Vec &syn1neg_target = params[target].h();
                        //}

						f = 0;
						for (c = 0; c < len_vec; c++)
                            f += neu1[c] * syn1neg_target[c];
							//f += neu1[c] * syn1neg[c + l2];
						if (f > MAX_EXP)
							g = (label - 1) * learning_rate;
						else if (f < -MAX_EXP)
							g = (label - 0) * learning_rate;
						else
							g = (label
							        - expTable[(int) ((f + MAX_EXP)
							                * (EXP_TABLE_SIZE / MAX_EXP / 2))])
							        * learning_rate;
                        // accumerate error
                        global_error.accu(10000* g * g);
                        
						for (c = 0; c < len_vec; c++)
                            neu1e[c] += g * syn1neg_target[c];
							//neu1e[c] += g * syn1neg[c + l2];
                        CHECK(grads.count(target) > 0) << "key:\t" << target;
                        auto& target_grad = grads[target]; 
                        /*
                        LOG(INFO) << "update target:\t" << target;
                        LOG(INFO) << "before update:\t" << target_grad;
                        LOG(INFO) << "grad:\t" << g * neu1;
                        LOG(INFO) << "g:\t" << g;
                        LOG(INFO) << "neu1:\t" << neu1;
                        */
                        target_grad.accu_h(g * neu1);// / alpha);
                        //LOG(INFO) << "after update:\t" << target_grad;
					}
				// hidden -> in
				for (a = b; a < window * 2 + 1 - b; a++)
					if (a != window) {
						c = sentence_position - window + a;
						if (c < 0)
							continue;
						if (c >= sentence_length)
							continue;
						last_word = sen[c];
						if (last_word == -1 || last_word == -2)
							continue;
                        //Vec &syn0_lastword = params[last_word].v();
                        CHECK(grads.count(last_word) > 0) << "key:\t" << last_word;
                        auto& grad_lastword = grads[last_word];
                        grad_lastword.accu_v(neu1e);// / alpha);
					}
			}
			sentence_position++;
			if (sentence_position >= sentence_length) {
				sentence_length = 0;
				continue;
			}
		}
		fclose(fi);
		//free(neu1);
		//free(neu1e);
		//pthread_exit(NULL);
        RAW_DLOG(INFO, "train thread terminated!");
	}

    void init_exp_table() {
        expTable = (real_t *) malloc((EXP_TABLE_SIZE + 1) * sizeof(real_t));
        if (expTable == NULL) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }
        for (int i = 0; i < EXP_TABLE_SIZE; i++) {
            expTable[i] = exp((i / (real_t) EXP_TABLE_SIZE * 2 - 1) * MAX_EXP); // Precompute the exp() table
            expTable[i] = expTable[i] / (expTable[i] + 1); // Precompute f(x) = x / (x + 1)
	    }
    }

private:
	Vocab global_vocab;
	int min_len = 0;
	int len_vec = 0;
	//bool local_train = false;
	int num_threads = 0;
	int window = 0;
	std::random_device rd;
	int negative;
    int num_sents_to_cache = 0;
	bool cbow = false;
	real_t alpha = 0.025;
    real_t learning_rate = 0.025;
    real_t sample = 1e-3;
    bool debug_mode = true;
    real_t starting_alpha = 0.025;
    real_t *expTable;
    string data_path;
};
