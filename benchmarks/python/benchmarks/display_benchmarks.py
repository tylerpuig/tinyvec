import pandas as pd
import plotly.express as px
from plotly.subplots import make_subplots
import plotly.graph_objects as go
import json

FILE_PATH = "metrics.json"


def fmt_ms(ms: int) -> int:
    if "node" in FILE_PATH:
        return ms
    return ms * 1000


# Read data
with open(FILE_PATH, 'r') as f:
    data = json.load(f)

# Process data
processed_rows = []
for run in data:
    row = {
        'Database': run['database_title'],
        'Query Time (ms)': fmt_ms(run['query_time']),
        'RSS Δ (MB)': run['final_memory']['rss_mb'] - run['initial_memory']['rss_mb'],
        'VMS Δ (MB)': run['final_memory']['vms_mb'] - run['initial_memory']['vms_mb']
    }
    processed_rows.append(row)
df = pd.DataFrame(processed_rows)

# Calculate averages and sort by query time
avg_df = df.groupby('Database').agg({
    'Query Time (ms)': 'mean',
    'RSS Δ (MB)': 'mean',
    'VMS Δ (MB)': 'mean'
}).round(2).sort_values('Query Time (ms)')

# Create a single figure with subplots
fig = make_subplots(
    rows=2, cols=2,
    subplot_titles=('Average Query Time (ms)',
                    'RSS Memory Change (MB)',
                    'VMS Memory Change (MB)',
                    'Summary Table'),
    specs=[[{"type": "bar"}, {"type": "bar"}],
           [{"type": "bar"}, {"type": "table"}]]
)

# Define high-contrast colors for dark mode
bar_colors = ['#00ff87', '#00bfff', '#ff69b4', '#ffd700']

# Add bar plots
metrics = ['Query Time (ms)', 'RSS Δ (MB)', 'VMS Δ (MB)']
positions = [(1, 1), (1, 2), (2, 1)]

for i, (metric, pos) in enumerate(zip(metrics, positions)):
    fig.add_trace(
        go.Bar(
            x=avg_df.index,
            y=avg_df[metric],
            name=metric,
            showlegend=False,
            marker_color=bar_colors[i]
        ),
        row=pos[0], col=pos[1]
    )

# Add table
table_data = avg_df.reset_index()
pythonCopytable_data = avg_df.reset_index()
fig.add_trace(
    go.Table(
        header=dict(
            values=['Database'] + list(avg_df.columns),
            align='left',
            fill_color='#000000',
            font=dict(color='#00bfff', size=14),
            line_color='#404040'
        ),
        cells=dict(
            values=[table_data[col] for col in table_data.columns],
            align='left',
            fill_color=(['#000000'] * len(table_data)),  # Make each row black
            font=dict(color='#00bfff', size=12),
            line_color='#404040'
        ),
        columnwidth=[150, 100, 100, 100]
    ),
    row=2, col=2
)

# Update layout
fig.update_layout(
    height=800,
    width=1200,
    showlegend=False,
    title_text="Database Benchmark Results",
    title_x=0.5,
    font=dict(color='white'),
    plot_bgcolor='#1f1f1f',
    paper_bgcolor='#121212',
    title_font=dict(size=24, color='white')
)

# Update axes styling
fig.update_xaxes(
    gridcolor='#404040',
    tickfont=dict(color='white'),
    title_font=dict(color='white')
)

fig.update_yaxes(
    gridcolor='#404040',
    tickfont=dict(color='white'),
    title_font=dict(color='white')
)

# Update y-axes titles
for row, col in [(1, 1), (1, 2), (2, 1)]:
    fig.update_yaxes(
        title_text="Milliseconds" if row == 1 and col == 1 else "Megabytes",
        title_font=dict(color='white', size=14),
        row=row, col=col
    )

# Update annotations (subplot titles)
fig.update_annotations(font=dict(size=16, color='white'))

# Show figure and save
fig.show()
fig.write_html("combined_benchmark_results.html")

# Print results
print("\nAverage Results per Database:")
print(avg_df)
