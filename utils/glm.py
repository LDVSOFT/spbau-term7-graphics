import gdb.printing

class GLMVec3Printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return '(%+g, %+g, %+g)' % (self.val['x'], self.val['y'], self.val['z'])

class GLMVec4Printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return '(%+g, %+g, %+g, %+g)' % (self.val['x'], self.val['y'], self.val['z'], self.val['w'])

def build():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("glm")
    pp.add_printer('vec3', '^glm::tvec3<float', GLMVec3Printer)
    pp.add_printer('vec4', '^glm::tvec4<float', GLMVec4Printer)
    return pp

gdb.printing.register_pretty_printer(
    gdb.current_objfile(),
    build()
)
