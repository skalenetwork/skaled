# Skaled disk activity and crash resistance
This page documents all the places in skaled (not consensus!) code where it touches disk storage, proofs the cases where it can recover from sudden crash before some write to disk and describes cases where it cannot.

## Managed disk access
There are 3 parts where skaled can recover:

- managing state DB
- storing blocks and extras in blocks_and_extras DB
- managing block rotation (rotation of 4 DBs in blocks_and_extras)

Each place in the code where commit to DB happens has it’s unique ID. We will use these IDs in the program flow description to be sure that crashes can be handled properly.



| Num. 	| Step                                                                                                                                                                                                                                                                                                                                           	| Place of code                                                                                                             	| Commit ID                                                                                                           	| Status                 	|
|------	|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------	|---------------------------------------------------------------------------------------------------------------------------	|---------------------------------------------------------------------------------------------------------------------	|----------------------	|
| 1    	| At start, rotating DB blocks_and_extras is opened. If more than one DB piece is marked as “current” - then do recovery by removing this mark from another one.                                                                                                                                                                                 	| ` rotating_db_io::recover ` ` rotating_db_io::rotating_db_io`                                                             	| after_pieces_kill (issued after each kill) after_recover (issued after whole recover procedure)                     	| recovery flow        	|
| 2    	| (not used in actual code) if starting from “clean DB” - state DB is explicitly cleared                                                                                                                                                                                                                                                         	| ` OverlayDB::clearDB`                                                                                                     	| clearDB                                                                                                             	| currently irrelevant 	|
| 3    	| If started from empty blockchain - insert genesis block TODO WithExisting::Kill and WithExisting::Verify may introduce another behavior                                                                                                                                                                                                        	| `BlockChain::open`                                                                                                        	| insert_genesis                                                                                                      	| normal program flow  	|
| 4    	| If current DB piece doesn’t contain ‘pieceUsageBytes’ - it means that there was crash in previous rotation - and we need to fix new DB piece: 1. re-insert genesis 2. insert 0 pieceUsageBytes                                                                                                                                                 	| `BlockChain::open`                                                                                                        	| fix_bad_rotation                                                                                                    	| recovery flow        	|
| 5    	| (not relevant, but historically important) If existing DB was created by skaled with broken block rotation - then recompute DB usage by blocks and store it in DB (version appproximately strictly before 3.7.1)                                                                                                                               	| `BlockChain::recomputeExistingOccupiedSpaceForBlockRotation`                                                              	| recompute_piece_usage                                                                                               	| currently irrelevant 	|
| 6    	| New agreed block comes from consensus                                                                                                                                                                                                                                                                                                          	| `SkaleHost::createBlock`, `Client::importTransactionsAsBlock → syncTransactions, sealUnconditionally, importWorkingBlock` 	|                                                                                                                     	| normal program flow  	|
| 7    	| Transactions are executed 1-by-1; some of them are reverted NOTE! When committing 1st transaction, all accumulated “partial” receipts from previous block are cleared up! (safe cache for receipts)                                                                                                                                            	| Block::execute, State::execute m_state.` clearPartialTransactionReceipts();`                                              	| OverlayDB_comit_N (commit of all state changes from transaction) N = sequential number of commit: 1,2,3…..          	| normal program flow  	|
| 8    	| Block is written to DB                                                                                                                                                                                                                                                                                                                         	| `BlockChain::insertBlockAndExtras`                                                                                        	| insertBlockAndExtras                                                                                                	| normal program flow  	|
| 9    	| If block cannot be written to current DB piece - DB rotation is performed before it is written: 1. Directory with the oldest DB is removed 2. new leveldb in this directory is created and opened 3. new DB is marked as “current” 4. old DB is unmarked as current 5. genesis block is re-inserted to new DB piece (to not to rotate it out!) 	| ` BlockChain::rotateDBIfNeeded rotating_db_io::rotate`                                                                    	| 1. after_remove_oldest 2. after_open_leveldb 3. with_two_keys 4. genesis_after_rotate 5. after_genesis_after_rotate 	| normal program flow  	|

## Unmanaged disk access

| Action                                        	| Place in code                                                                                   	| Notes                           	|
|-----------------------------------------------	|-------------------------------------------------------------------------------------------------	|---------------------------------	|
| Snapshots creation                            	| SnapshotManager::doSnapshot                                                                     	|                                 	|
| Snapshot hash computing (not implemented yet) 	| SnapshotManager::computeSnapshotHash                                                            	| To do: need to create tests     	|
| Other snapshot operations                     	| SnapshotManager::restoreSnapshot SnapshotManager::removeSnapshot SnapshotManager::makeOrGetDiff 	|                                 	|
| Node rotation  (not implemented yet)          	|                                                                                                 	|                                 	|
| Filestorage operation  (not implemented yet)  	|                                                                                                 	| To do: Use `UnsafeRegion` (tbd) 	|
| Personal API (not implemented yet)            	| KeyManager;  AccountHolder                                                                      	| Not used on MainNet             	|












