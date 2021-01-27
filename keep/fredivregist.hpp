#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include "dividend.hpp"     // Const parameters for dividend 

using namespace std;
using namespace eosio; 

CONTRACT freeosdivide : public contract {
  public:
    using contract::contract;

    // After finishing the tests the next two actions below may be called only by multisig. 
    ACTION allowance( uint8_t code, name vipname ); // Creating whitelist of allowed proposer and voters.
    ACTION removeallow();                           // Removes the white_list table.
    // Helpers for tests: 
    ACTION clear(name tscope);                      // Clean up the mess after the tests.  
    ACTION votesreset11(name user);                 // Clean up voting results (enforced)
    ACTION regchown(name userfrom, name userto, uint64_t nft_id); //Change NFT ownership (NFT transfer).  Add Memo??

    ACTION proposalnew(
  		  const name    proposer,
        const name    eosaccount,          
        const uint8_t roi_target_cap,      
        const double  weekly_percentage,
        const asset   threshold,      
        const uint32_t  rates_left,        
        const bool    locked ); 

    ACTION proposalvote(  
        const name voter,
        const uint8_t vote );

    ACTION dividcomputed(); 
    ACTION regtransfer(); 
    ACTION zerofortest();                 // TEST only - helper to remove previous tests results - to remove later 

    void notify_front( uint8_t number );        
    void clearfront();  
    void prop_reset();    
    void vote_reset(); 

  private:

    static inline time_point_sec current_time_point_sec() {
      return time_point_sec(current_time_point());
    }

    uint32_t now() {
      return current_time_point().sec_since_epoch();
    }

  TABLE status_messages {                 // This keepd the error numbers for the latest proposal to be interpreted by the frontend. 
                                          // New proposal erases this list.
    uint64_t key;
    uint8_t  errorno;
    auto primary_key() const { return key; }
  };
  using messages_table = eosio::multi_index<"messages"_n, status_messages>;

  TABLE proposal_struct {                     
      uint64_t key;                       // primary key = always 1.
      name eosaccount;                    //!< freeos account used to receive dividends and for identification
      uint8_t roi_target_cap;             
      double weekly_percentage;           //percentage
      bool locked;                        //!< lock dividends for selected new members. Note: When unlock cannot be locked again.
      uint32_t expires_at;                //!< expiry for the proposal only (must be voted before that time)
      asset threshold;                    //!< max total divident (2) for horizontal cap or max weekly dividend for vertical (3) cap 
      uint32_t rates_left;                //!< count down payments left in iteration cap (I) only  
      asset accrued;
      uint64_t primary_key() const { return key; }
  };
  typedef eosio::multi_index<"proposals"_n, proposal_struct> proposal_table;

  TABLE whitelist_struct {                 // !< whitelist_struct - List of allowed proposers and voters along with their vote.
      uint64_t idno;                       // (1-proposer, 2,3 voters)
      name     user;                       
      uint8_t  vote;                       //!< 0 -n/a  1 - no   2 - yes /     - different than zero if already voted
      uint64_t primary_key() const { return user.value; }   //!< This table should be filled up only by a real multisig
      uint64_t get_secondary_1() const { return idno; }
	};
  using whitelist_index = eosio::multi_index<"whitelist"_n, whitelist_struct,
    indexed_by<"byidno"_n, const_mem_fun<whitelist_struct, uint64_t, &whitelist_struct::get_secondary_1>>>;

  TABLE total_struct {
      name     user;
      asset    to_receive;  //This is a cumulative amount of FREEOS tokens which will be paid to the user on a basis of all owned NFTs. 
      uint64_t primary_key() const {return user.value; }
  };
  using total_index = eosio::multi_index<"totals"_n, total_struct>;  


  TABLE register_struct {                   // This register record is single NFT by itself
      uint64_t nft_id;                      // One person may have several NFTs - nft_id is primary key with autoincrement
      name     eosaccount;                  //!< freeos account used to receive dividends and for identification (as a secondary key)
      uint8_t  roi_target_cap;              // 1-     2-    3- 
      double   weekly_percentage;           // Only this is used for counting dividend to pay - the other parameters wxamine eligibility 
                                            // for payment and it's size. 
      time_point_sec mint_date;             // current date (when this register record was created)
      bool     locked;                      //!< lock dividends for selected new members. Note: When unlock should be not lock again.
      asset    threshold = asset(0,symbol("FREEOS",4) ); //!< max total divident (2) for horizontal cap or max weekly dividend for vertical (3) cap
      uint32_t rates_left;                  //!< count down payments left in iteration cap (I) only   
      asset    accrued = asset(0,symbol("FREEOS",4) ); ;                       
      uint64_t primary_key() const {return nft_id;}
      uint64_t get_secondary_1() const { return eosaccount.value;}
  };
  using register_table = eosio::multi_index<"registers"_n, register_struct,
    indexed_by<"byacct"_n, const_mem_fun<register_struct, uint64_t, &register_struct::get_secondary_1>>>;      
 
 
};


