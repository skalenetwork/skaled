#!/bin/bash

# Set the process ID (PID) to monitor
PROCESS_PID=47822  # Replace with your process's PID

# Check if the PID is valid
if ! ps -p $PROCESS_PID > /dev/null 2>&1; then
    echo "Process with PID $PROCESS_PID not found."
    exit 1
fi

# File to store memory usage data
DATA_FILE="rss_memory_usage.dat"
PLOT_FILE="rss_memory_plot.png"

# Initialize the data file
echo "# Time(s) RSS(MB)" > $DATA_FILE

# Function to get the current memory usage of the process
get_rss_memory_usage() {
    # Get the RSS memory in kilobytes and convert to megabytes
    ps -p $PROCESS_PID -o rss= | awk '{ printf "%.2f", $1/1024 }'
}

# Start time in seconds
START_TIME=$(date +%s)

# Trap to handle script interruption (e.g., Ctrl+C)
trap "echo; echo 'Plotting data...'; plot_data; exit 0" SIGINT

# Function to plot data using gnuplot
plot_data() {
    gnuplot -persist <<-EOFMarker
        set terminal png size 800,600
        set output "$PLOT_FILE"
        set title "RSS Memory Usage of Process PID $PROCESS_PID"
        set xlabel "Time (s)"
        set ylabel "RSS Memory (MB)"
        set grid
        plot "$DATA_FILE" using 1:2 with lines title "RSS Memory"
EOFMarker
    echo "Plot saved to $PLOT_FILE"
}

# Main loop to collect memory usage data over time
while true; do
    CURRENT_TIME=$(date +%s)
    ELAPSED_TIME=$((CURRENT_TIME - START_TIME))
    RSS_USAGE=$(get_rss_memory_usage)
    echo "$ELAPSED_TIME $RSS_USAGE" >> $DATA_FILE
    echo "Time: $ELAPSED_TIME s, RSS Memory: $RSS_USAGE MB"
    sleep 1  # Collect data every second
done

