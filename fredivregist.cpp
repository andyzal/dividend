// Last Edited: 1st Nov. by A.Zaliwski License: MIT
#include <fredivregist.hpp>

// Create whitelist
    ACTION fredivregist::allowance( uint64_t key, name vipname ){
      require_auth( get_self() );

      whitelist_index white_list(get_self(), get_self().value);

      auto iterator = white_list.find(vipname.value);
          if( iterator == white_list.end() )
          {
            white_list.emplace(vipname, [&]( auto& row )
            {
              row.key = key;
              row.vip_user = vipname;
              row.vote = 0;
            });
          }
          else {
            white_list.modify(iterator, vipname, [&]( auto& row ) {
              row.key = key;
              row.vip_user = vipname;
              row.vote = 0;
             });
          }
    }



      /*-------------------
       +  proposal_create
       +-------------------
              +
              +  Proposal table is filled-up by the data coming from the frontend.
              +      - only valid proposer can create the proposal.
              */

      ACTION fredivregist::proposalnew(
        // Proposer entered parameters
        const name proposer,
        const name eosaccount,                    //!< freeos account used to receive dividends and for identification
        const char policy_name,                   //!< policy_name - (a)WayFarer or (b)WayFinder
        const char user_type,                     //!< user_type -(f)ounder or (i)nvestor
        const uint64_t usd_investment,
        const uint32_t target_freeos_price,       //TODO verify types/  const questionable?
        const uint8_t roi_target_cap,
        const uint8_t weekly_percentage,
        const bool locked,                        //!< lock dividends for selected new members. Note: When unlock cannot be locked again.
        const uint64_t rates_left,                // number of payments left under this policy (apply only to specific variants
      ){
        require_auth(proposer);
        // Is proposer white-listed for this action?
        whitelist_index white_list(get_self(), get_self().value);
        check(white_list.find(proposer.value) == white_list.end(), " :( VIP_list: You have no rights to perform this operation!!");
        //check(expires_at > current_time_point_sec(), "expires_at must be a value in the future.");
        // Assume blank proposal_struct
        const uint64_t key = 1;
        proposal_table proposals(get_self(), get_self().value);
        check(proposals.find(key) == proposals.end(), " :( A Proposal already exists");
        auto p = proposals.find(key);
        if ( p == proposals.end() ){
          //create new proposal
          proposals.emplace(_self, [&](auto &p) {
                      p.key = 1; // Always one, if no more than one proposal at once allowed.
                      p.eosaccount          = eosaccount;
                      p.policy_name         = policy_name;
                      p.user_type           = user_type;
                      p.usd_investment      = usd_investment;
                      p.target_freeos_price = target_freeos_price;
                      p.roi_target_cap      = roi_target_cap;
                      p.weekly_percentage   = weekly_percentage;
                      p.locked              = locked;
                      p.expires_at = current_time_point_sec() + EXPIRATION_PERIOD;
           });
        } //end of if
        // Notify voters using frontend when this proposal is submitted.
      } // --- end of proposal_create ---


      /*
       +-------------------
       +  proposal_clear
       +-------------------
                +
                +  Recycle RAM after proposal not longer needed.
                +
                */
// wrapper
ACTION fredivregist::proposalclr( name proposer) {
    require_auth(proposer);
    proposaldestroy();
}

// enforced expiration of the proposal
ACTION fredivregist::expire( name proposer ) {
    require_auth(proposer);
    // is proposer on white-list
    whitelist_index white_list(_self, _self.value);
    auto v = white_list.find(proposer.value);
    if ( v == white_list.end() ){
        eosio::printf("(VIP_list): You have no rights for this operation!!");
        notify_front( 1 );
        return;
    } else { //proposer accepted
      // make proposal expired
      proposal_table proposals(_self, _self.value);
      const uint64_t key = 1;
      auto itr = proposals.find(key);
      check(itr != proposals.end(), "proposal not found.");
      check(!itr->is_expired(), "proposal is already expired."); //???
      proposals.modify(itr, proposer, [&](auto& row) {
        row.expires_at = current_time_point_sec();
      });
    }
} //end of.


      /*
      +-------------------
      +  proposal_vote
      +-------------------
              +
              +  Voting for the proposal. If this is last vote and both votes are positive, the proposal is written
              +  to the register. Otherwise the proposal will be destroyed.
              +
              */

  ACTION fredivregist::proposalvote(  name    voter,
                                      uint8_t vote   )
      {
        require_auth(voter);
        //voter allowed?
        whitelist_index white_list(_self, _self.value);
        auto v = white_list.find(voter.value);
        if ( v == white_list.end() ){ //wrong voter
            eosio::printf("Error 1 (VIP_list): You do not have rights for this operation!!");
            notify_front( 1 );
            return;
        }

        //voter is allowed
        check(v->key!=1, "You are proposer not voter!");
        //proposal expired?
        proposal_table proposals(get_self(), get_self().value);
        auto rec_itr = proposals.begin();
        check(rec_itr->expires_at > current_time_point_sec(), "proposal already expired.");  //Note function TODO

        //is it your first vote??
        check(v->vote==0, "This is your second vote!"); //assume: return if false (?)

        //proposal is not expired and voter is allowed - then update vote
        white_list.modify(v,_self,[&](auto& p){  p.vote = vote; }

        //verify proposal approval or refusal
        //
        uint8_t a, b;
        v = white_list.begin()
        v++; //ignore proposer  //Note this may not work due to issue in eosio
        a = v->vote; //take first voter result
        v++;
        b = v->vote; //take second voter result

        if ( (a==1)||(b==1) ) { //proposal refused - destroy it :(
          proposaldestroy();
        return;
        }

        if ( (a==2)&&(b==2) ) { //proposal accepted - finalize it :)
        //Copy proposal to the investors register (this is equal to minting NFT)
        rec_itr = proposal.begin();
        register_table register(_self, _self.value);
        auto reg_itr = register.end();
        register.emplace(_self, [&]( auto& r ){
          r.eosaccount              = rec_itr->eosaccount;
          r.policy_name             = rec_itr->policy_name;
          r.user_type               = rec_itr->user_type;
          r.usd_investment          = rec_itr->usd_investment;
          r.target_freeos_price     = rec_itr->target_freeos_price;
          r.roi_target_cap          = rec_itr->roi_target_cap;
          r.weekly_percentage       = rec_itr->weekly_percentage;
          //Note: This register record is single NFT by itsel
          r.mint_date               = current_time_point_sec();
          r.locked                  = rec_itr->locked;
          r.rates_left              = rec_itr->rates_left;
          //sharefraction - this is computed field (later) TODO discuss                                                     ;
        });
        eosio::printf("Proposal accepted and saved. NFT minted.");
        notify_front( 5 );
        proposaldestroy();
        }

      } //end.



      /* this is DIVIDEND FRAME
      +-------------------
      +  dividend compute
      +-------------------
              +
              +  Counting and write computing code (based on prescribed policy) to each investor record.
              +  The code is used in the next step to count the dividend.
              +
              */

  ACTION fredivregist::dividcomputed( name dividend_acct ){
    require_auth(_self);

  } //end.

      /* this is DIVIDEND FRAME
      +-------------------
      +  dividend delivery
      +-------------------
              +
              +  The action transfer eligible amount of tokens from pre-designed account to account of each investor
              +  according to pre-computed earlier code.
              +
              */

  ACTION fredivregist::dividdelivery( name dividend_acct ){
    require_auth(_self);

  } //end.



  ACTION fredivregist::dividendchown( name owner, name new_owner ){ // Change ownership
    //verify existence of owner account
    require_auth( owner );
    //verify existence of target (new owner) account

    //make Change


    //Notify


  }





      //                        //
      // Small Helper Functions //
      //                        //

// List of errors to export to the frontend for interpretation
void fredivregist::notify_front( uint8_t number ){
  messages_table errortable(_self, _self.value);
  auto e = errortable.end();
  errortable.emplace(_self, [&](auto &e) {
    e.key = e.key + 1; //verify TODO
    e.errorno = number;
  }
} //end of.

// Clear list of errors when start new proposal - no conditions verified
void fredivregist::clearfront( void ){
  messages_table errortable(_self, _self.value);
  auto e = errortable.begin();
  while (e != errortable.end()) {
      e = errortable.erase(rec_itr);
  }
} //end of.

// Clear/destroy proposal - Warning : this is only for internal use as it is unconditional.
void fredivregist::proposaldestroy( void ){
    proposal_table proposals(get_self(), get_self().value);
    // Delete proposal - release RAM
    auto rec_itr = proposals.begin();
    while (rec_itr != proposals.end()) {
      rec_itr = proposals.erase(rec_itr);
    }
}






EOSIO_DISPATCH(fredivregist, (allowance)(proposalnew)(proposalclr))  //not finished TODO
