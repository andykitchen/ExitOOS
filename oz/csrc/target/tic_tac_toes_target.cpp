#include "oos.h"

#include <iostream>
#include <sstream>
#include <string>
#include <bitset>

#include "tic_tac_toes_target.h"

namespace oz {
    
    
    using namespace std;
    using var_t = int[9];
    
    
    
    static auto cast_history(const history_t &h) -> const tic_tac_toes_t& {
        return h.cast<tic_tac_toes_t>();
    }

    static auto cast_infoset(const infoset_t &infoset) -> const tic_tac_toes_t::infoset_t& {
        return infoset.cast<tic_tac_toes_t::infoset_t>();
    }
    
    
    int action_index(const tic_tac_toes_t::action_t a){
      switch(a){
          case tic_tac_toes_t::action_t::fill_1 :
              return 0;
          case tic_tac_toes_t::action_t::fill_2 :
              return 1;
          case tic_tac_toes_t::action_t::fill_3 :
              return 2;
          case tic_tac_toes_t::action_t::fill_4 :
              return 3;
          case tic_tac_toes_t::action_t::fill_5 :
              return 4;
          case tic_tac_toes_t::action_t::fill_6 :
              return 5;
          case tic_tac_toes_t::action_t::fill_7 :
              return 6;
          case tic_tac_toes_t::action_t::fill_8 :
              return 7;
          case tic_tac_toes_t::action_t::fill_9 :
              return 8;
          case tic_tac_toes_t::action_t::NextRound :
              return -1;
          default: assert(false); 
              
          }
      }
    
    bool is_discovered(int a){
    
        bool discovered = false;
        return discovered;
    }
    
    
    void is_legal_move(int var[9], tic_tac_toes_t::action_vector_t previous_moves){
        
        unsigned int i;
        for(i = 0; i < previous_moves.size(); ++i){
            if (action_index(previous_moves[i]) >= 0)
                var[action_index(previous_moves[i])] = 0;
        }
        
    }
    
    
      
    bool is_winning_move(unsigned int tot_moves[9]) {
    
        unsigned int end_of_the_game = 0;

        if (tot_moves[0] == 1){
            /* case
            x - -
            x - -
            x - -
            */
            if (tot_moves[1] == 1 and tot_moves[2] == 1){
                end_of_the_game = 1;
            }
            /* case
            - - -
            - - -
            x x x
            */
            else if (tot_moves[3] == 1 and tot_moves[6] == 1){
                end_of_the_game = 1;
            }
            /* case
            - - x
            - x -
            x - -
            */
            else if (tot_moves[4] == 1 and tot_moves[8] == 1){
                end_of_the_game = 1;
            }
        }
        /* case
        - - -
        x x x
        - - -
        */
        if (tot_moves[1] == 1 and tot_moves[4] == 1 and tot_moves[7] == 1){
            end_of_the_game = 1;
        }
        if (tot_moves[2] == 1) {
            /* case
            x x x
            - - -
            - - -
            */
            if (tot_moves[5] == 1 and tot_moves[8] == 1){
                end_of_the_game = 1;
            }

            /* case
            x - -
            - x -
            - - x
            */
            else if (tot_moves[4] == 1 and tot_moves[6] == 1){
                end_of_the_game = 1;
            }
        }

        /* case
        - x -
        - x -
        - x -
        */
        if (tot_moves[3] == 1 and tot_moves[4] == 1 and tot_moves[5] == 1){
            end_of_the_game = 1;
        }

        /* case
        - - x
        - - x
        - - x
        */
        if (tot_moves[6] == 1 and tot_moves[7] == 1 and tot_moves[8] == 1){
              end_of_the_game = 1;
        }


        return end_of_the_game;
    
    }
    
    
    
    void is_winning_move(int var[9], tic_tac_toes_t::action_vector_t previous_moves){
        
        unsigned int past_legal_moves[9] = {0,0,0,0,0,0,0,0,0};
        unsigned int i;
        unsigned int end_of_the_game = 0;
        
        for(i = 0; i < previous_moves.size(); ++i){
            
            // case NextRound
            if (action_index(previous_moves[i]) == -1)
                past_legal_moves[action_index(previous_moves[i - 1])] = 1;
        }
        
        for (i = 0; i < 9; i++){
            if (var[i] and past_legal_moves[i] == 0){
                past_legal_moves[i] = 1;
                bool flag_winning = is_winning_move( past_legal_moves);
                if (flag_winning == 1)
                    var[i] = 0;
                past_legal_moves[i] = 0;
            }
        }
        
    }
    
    
    void opponent_discovery_constraint(int var[9], unsigned int turn_number, const player_t player, tic_tac_toes_t::action_vector_t moves_P1, tic_tac_toes_t::action_vector_t moves_P2){
        
        /*if (player == P1){
            cout << "P1" << endl;
        }*/
        
        
        
    }
    
    void playable(const player_t player, tic_tac_toes_t::action_vector_t moves_P1, 
                         tic_tac_toes_t::action_vector_t moves_P2, int x[9], int turn_number) {
        
        int var[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
        
        if (player == P1){
            
            is_legal_move(var, moves_P1);
            is_winning_move(var, moves_P1);
            
        }
        else {
            
            is_legal_move(var, moves_P2);
            is_winning_move(var, moves_P2);
        }
        opponent_discovery_constraint(var, turn_number, player, moves_P1, moves_P2);
    
        
        
    }
    
    
    auto tic_tac_toes_target_t::target_actions(const infoset_t &target_infoset,const history_t &current_history) const
  -> set<action_t>{
        
        using action_t = tic_tac_toes_t::action_t;

        const auto &target_infoset_base = cast_infoset(target_infoset);
        const auto &target_player = target_infoset_base.player;

        const auto &moves_P1 = target_infoset_base.pieces_P1;
        const auto &moves_P2 = target_infoset_base.pieces_P2;

        const auto &current_game = cast_history(current_history);

        const auto &current_actions = current_game.history();
        const auto &target_actions = target_infoset_base.history;
        const auto next_ply_n = current_actions.size();
        unsigned int turn_number = 0;
        
        // calculate the turn number
        if (current_game.player() == P1){
            for (unsigned int i = 0; i < moves_P1.size(); i++)
                if (action_index(moves_P1[i]) == -1)
                    turn_number++;
            
        }
        else {
            for (unsigned int i = 0; i < moves_P1.size(); i++)
                if (action_index(moves_P1[i]) == -1)
                    turn_number++;
        }
        
        int x[9];
        
        if (current_actions.size() < target_actions.size()) {
            if (current_game.player() == target_player) {
                const auto target = target_actions[next_ply_n];
                //Ensures(target != action_t::NextRound);
                return { make_action(target) };
            }
            else {
                playable(current_game.player(), moves_P1, moves_P2, x, turn_number);
                return { };
            }
        }
        else {
            return { };
        }
    }

} // namespace oz
