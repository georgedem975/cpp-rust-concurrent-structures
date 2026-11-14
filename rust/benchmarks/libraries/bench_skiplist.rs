use std::sync::Arc;

use bench::{parse_options, run_grid};
use crossbeam_skiplist::{SkipMap, SkipSet};

const KEY_SPACE: u32 = 1_000_000;
const OPERATION_RANGE: u32 = 10;
const INSERT_LIMIT: u32 = 2;
const READ_LIMIT: u32 = 9;
const THREAD_SEED_MULTIPLIER: u32 = 0x9e37_79b9;
const MAP_SEED: u32 = 0x9e37_79b9;
const SET_SEED: u32 = 0x85eb_ca6b;

#[inline]
fn next_random(state: &mut u32) -> u32 {
    *state ^= *state << 13;
    *state ^= *state >> 17;
    *state ^= *state << 5;
    *state
}

#[inline]
fn next_key(state: &mut u32) -> u32 {
    next_random(state) % KEY_SPACE
}

fn seed_for_thread(seed: u32, thread_id: usize) -> u32 {
    let seed = seed ^ (thread_id as u32 + 1).wrapping_mul(THREAD_SEED_MULTIPLIER);
    seed.max(1)
}

fn run_map_batch(map: &SkipMap<u32, u32>, batch_size: u64, random_state: &mut u32) {
    for _ in 0..batch_size {
        let key = next_key(random_state);
        match next_random(random_state) % OPERATION_RANGE {
            roll if roll < INSERT_LIMIT => {
                map.insert(key, key);
            }
            roll if roll < READ_LIMIT => {
                let _ = map.get(&key);
            }
            _ => {
                let _ = map.remove(&key);
            }
        }
    }
}

fn run_set_batch(set: &SkipSet<u32>, batch_size: u64, random_state: &mut u32) {
    for _ in 0..batch_size {
        let key = next_key(random_state);
        match next_random(random_state) % OPERATION_RANGE {
            roll if roll < INSERT_LIMIT => {
                set.insert(key);
            }
            roll if roll < READ_LIMIT => {
                let _ = set.contains(&key);
            }
            _ => {
                let _ = set.remove(&key);
            }
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let options = parse_options();
    let batch_size = options.batch;

    let map = Arc::new(SkipMap::new());
    let set = Arc::new(SkipSet::new());
    for key in 0..KEY_SPACE {
        map.insert(key, key);
        set.insert(key);
    }

    let map_for_workers = Arc::clone(&map);
    run_grid(
        &options,
        "rust",
        "skipmap",
        "mix_20_70_10",
        move |context| {
            let map = Arc::clone(&map_for_workers);
            let mut random_state = seed_for_thread(MAP_SEED, context.id);
            Box::new(move || run_map_batch(&map, batch_size, &mut random_state))
        },
    )?;

    let set_for_workers = Arc::clone(&set);
    run_grid(
        &options,
        "rust",
        "skipset",
        "mix_20_70_10",
        move |context| {
            let set = Arc::clone(&set_for_workers);
            let mut random_state = seed_for_thread(SET_SEED, context.id);
            Box::new(move || run_set_batch(&set, batch_size, &mut random_state))
        },
    )?;

    Ok(())
}