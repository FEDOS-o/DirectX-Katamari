cbuffer TransformBuffer : register(b0)
{
    float4x4 transformMatrix;
};

struct VS_IN
{
    float4 pos : POSITION;
    float4 col : COLOR;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    
    output.pos = mul(input.pos, transformMatrix);
    output.col = input.col;
    
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 col = input.col;
    
#ifdef TEST
    // Проверяем позицию по X (в координатах от -1 до 1)
    // Конвертируем в экранные координаты (предполагаем ширину 800)
    float screenX = (input.pos.x + 1.0f) * 400.0f;
    if (screenX > 400.0f) {
        col = float4(0.0f, 1.0f, 0.0f, 1.0f); // Ярко-зеленый
    }
#endif

    return col;
}