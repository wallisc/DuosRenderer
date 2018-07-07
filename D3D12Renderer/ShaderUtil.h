#define GLUE(a,b) a##b
#define REGISTER_SPACE(spaceNum) GLUE(space,spaceNum)
#define DECLARE_REGISTER(prefix, regNum, spaceNum) register(GLUE(prefix,regNum), REGISTER_SPACE(spaceNum))

#define CONSTANT_REGISTER_SPACE(regNum, spaceNum) DECLARE_REGISTER(b, regNum, spaceNum)
#define CONSTANT_REGISTER(regNum) CONSTANT_REGISTER_SPACE(regNum, 0)
#define SRV_REGISTER_SPACE(regNum, spaceNum) DECLARE_REGISTER(t, regNum, spaceNum)
#define SRV_REGISTER(regNum) SRV_REGISTER_SPACE(regNum, 0)
#define SAMPLER_REGISTER_SPACE(regNum, spaceNum) DECLARE_REGISTER(s, regNum, spaceNum)
#define SAMPLER_REGISTER(regNum) SAMPLER_REGISTER_SPACE(regNum, 0)
#define UAV_REGISTER_SPACE(regNum, spaceNum) DECLARE_REGISTER(u, regNum, spaceNum)
#define UAV_REGISTER(regNum) UAV_REGISTER_SPACE(regNum, 0)