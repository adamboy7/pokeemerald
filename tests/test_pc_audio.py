import pytest

PCM_DMA_BUF_SIZE = 1584
PCM_SAMPLES_PER_VBLANK_TABLE = [
    96, 132, 176, 224, 264, 304, 352, 448, 528, 608, 672, 704,
]

FREQ_CASES = [
    (0x00010000, 5734),
    (0x00020000, 7884),
    (0x00030000, 10512),
    (0x00040000, 13379),
    (0x00050000, 15768),
    (0x00060000, 18157),
    (0x00070000, 21024),
    (0x00080000, 26758),
    (0x00090000, 31536),
    (0x000A0000, 36314),
    (0x000B0000, 40137),
    (0x000C0000, 42048),
]

def calculate_params(freq_const):
    freq_index = (freq_const & 0xF0000) >> 16
    samples = PCM_SAMPLES_PER_VBLANK_TABLE[freq_index - 1]
    period = PCM_DMA_BUF_SIZE // samples
    pcm_freq = (597275 * samples + 5000) // 10000
    div_freq = (16777216 // pcm_freq + 1) >> 1
    return samples, period, pcm_freq, div_freq

@pytest.mark.parametrize("freq_const,expected_pcm", FREQ_CASES)
def test_pcm_frequency(freq_const, expected_pcm):
    _, _, pcm_freq, _ = calculate_params(freq_const)
    assert pcm_freq == expected_pcm

@pytest.mark.parametrize("freq_const,_", FREQ_CASES)
def test_dma_period(freq_const, _):
    samples, period, _, _ = calculate_params(freq_const)
    assert period == PCM_DMA_BUF_SIZE // samples

@pytest.mark.parametrize("freq_const,_", FREQ_CASES)
def test_samples_per_vblank_mapping(freq_const, _):
    freq_index = (freq_const & 0xF0000) >> 16
    samples, _, _, _ = calculate_params(freq_const)
    assert samples == PCM_SAMPLES_PER_VBLANK_TABLE[freq_index - 1]
