cbuffer cv_viewport : register(b0)
{
    row_major float2x3 rotation_matrix;
}

struct vs_input
{
    float2 position : POSITION;
    float4 color : COLOR;
};

struct ps_input
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

ps_input main_vs(vs_input input )
{
    ps_input output;
    float2 p = input.position;
    p = mul(rotation_matrix, float3(input.position, 1.0f));
    output.position = float4(p, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}

float4 main_ps(ps_input input) : SV_TARGET
{
    return input.color;
}
