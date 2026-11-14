#[derive(Clone, Debug, Default)]
pub struct Row {
    pub language: String,
    pub primitive: String,
    pub scenario: String,
    pub threads: usize,
    pub repeat: usize,
    pub batch: u64,
    pub metric: Option<Metric>,
    pub ops: u64,
    pub wall_ns: f64,
    pub throughput: f64,
    pub lat_avg_ns: f64,
    pub lat_p50_ns: f64,
    pub lat_p95_ns: f64,
    pub lat_p99_ns: f64,
    pub lat_p999_ns: f64,
    pub lat_max_ns: f64,
}

const NANOS_PER_SECOND: f64 = 1_000_000_000.0;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Metric {
    Throughput,
    Latency,
}

impl Row {
    pub fn calculate_throughput(&mut self) {
        self.throughput = if self.wall_ns > 0.0 {
            self.ops as f64 * NANOS_PER_SECOND / self.wall_ns
        } else {
            0.0
        };
    }

    pub fn calculate_latency(&mut self, samples: &mut [f64]) {
        assert!(self.batch > 0, "batch must be greater than zero");
        assert!(
            samples.iter().all(|sample| sample.is_finite()),
            "latency samples must be finite"
        );

        let batch = self.batch as f64;

        for sample in samples.iter_mut() {
            *sample /= batch;
        }

        samples.sort_by(f64::total_cmp);

        self.lat_avg_ns = if samples.is_empty() {
            0.0
        } else {
            samples.iter().sum::<f64>() / samples.len() as f64
        };

        self.lat_p50_ns = percentile(samples, 0.50);
        self.lat_p95_ns = percentile(samples, 0.95);
        self.lat_p99_ns = percentile(samples, 0.99);
        self.lat_p999_ns = percentile(samples, 0.999);
        self.lat_max_ns = samples.last().copied().unwrap_or(0.0);
    }
}

fn percentile(values: &[f64], fraction: f64) -> f64 {
    if values.is_empty() {
        return 0.0;
    }

    let position = fraction * (values.len() - 1) as f64;
    let lower = position as usize;
    let upper = (lower + 1).min(values.len() - 1);
    let weight = position - lower as f64;

    values[lower] * (1.0 - weight) + values[upper] * weight
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn calculates_throughput_from_operations_and_wall_time() {
        let mut row = Row {
            ops: 200,
            wall_ns: 2_000_000_000.0,
            ..Default::default()
        };

        row.calculate_throughput();

        assert_eq!(row.throughput, 100.0);
    }

    #[test]
    fn calculates_latency_statistics_using_interpolated_percentiles() {
        let mut row = Row {
            batch: 1,
            ..Default::default()
        };

        let mut samples = [10.0, 40.0, 20.0, 30.0];

        row.calculate_latency(&mut samples);

        assert_eq!(row.lat_avg_ns, 25.0);
        assert_eq!(row.lat_p50_ns, 25.0);
        assert_eq!(row.lat_p95_ns, 38.5);
        assert!((row.lat_p99_ns - 39.7).abs() < 1e-12);
        assert!((row.lat_p999_ns - 39.97).abs() < 1e-12);
        assert_eq!(row.lat_max_ns, 40.0);
    }

    #[test]
    fn calculates_latency_for_batch_samples() {
        let mut row = Row {
            batch: 10,
            ..Default::default()
        };

        let mut samples = [100.0, 200.0, 300.0];

        row.calculate_latency(&mut samples);

        assert_eq!(row.lat_avg_ns, 20.0);
        assert_eq!(row.lat_p50_ns, 20.0);
        assert_eq!(row.lat_max_ns, 30.0);
    }

    #[test]
    fn empty_latency_samples_produce_zero_statistics() {
        let mut row = Row {
            batch: 1,
            ..Default::default()
        };

        let mut samples = [];

        row.calculate_latency(&mut samples);

        assert_eq!(row.lat_avg_ns, 0.0);
        assert_eq!(row.lat_max_ns, 0.0);
    }

    #[test]
    #[should_panic(expected = "batch must be greater than zero")]
    fn rejects_zero_batch() {
        let mut row = Row::default();
        let mut samples = [10.0];

        row.calculate_latency(&mut samples);
    }
}